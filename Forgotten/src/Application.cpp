#include "Application.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <cstdio> // printf, fprintf
#include <cstdlib> // abort
#include <queue>
#include <utility>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// Emedded font
#include "Timer.hpp"
#include "imgui/Roboto-Regular.embed"
#include "vulkan/VulkanContext.hpp"

extern bool is_application_running;

#define IMGUI_UNLIMITED_FRAME_RATE
#define IMGUI_VULKAN_DEBUG_REPORT

static VkDebugReportCallbackEXT global_debug_report = VK_NULL_HANDLE;
static VkPipelineCache global_pipeline_cache = VK_NULL_HANDLE;
static VkDescriptorPool global_descriptor_pool = VK_NULL_HANDLE;

static float frame_time{ 0.0f };
static float average_fps{ 0.0f };

static ImGui_ImplVulkanH_Window global_main_window_data;
static int global_min_image_count = 2;
static bool global_swap_chain_rebuild = false;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>> allocated_framebuffers;
static std::vector<std::vector<std::function<void()>>> resource_free_queue;

// Unlike global_main_window_data.FrameIndex, this is not the the swapchain image index
// and is always guaranteed to increase (eg. 0, 1, 2, 0, 1, 2)
static uint32_t s_CurrentFrameIndex = 0;

void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
	const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	(void)flags;
	(void)object;
	(void)location;
	(void)messageCode;
	(void)pUserData;
	(void)pLayerPrefix; // Unused arguments
	fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
	return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void setup_vulkan_window(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	wd->Surface = surface;

	// Check for WSI support
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(ForgottenEngine::VulkanContext::get_physical_device(),
		ForgottenEngine::VulkanContext::get_queue_family(), wd->Surface, &res);
	if (res != VK_TRUE) {
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}

	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[]
		= { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->SurfaceFormat
		= ImGui_ImplVulkanH_SelectSurfaceFormat(ForgottenEngine::VulkanContext::get_physical_device(), wd->Surface,
			requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(ForgottenEngine::VulkanContext::get_physical_device(),
		wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

	// Create SwapChain, RenderPass, Framebuffer, etc.
	IM_ASSERT(global_min_image_count >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(ForgottenEngine::VulkanContext::get_instance(),
		ForgottenEngine::VulkanContext::get_physical_device(), ForgottenEngine::VulkanContext::get_device(), wd,
		ForgottenEngine::VulkanContext::get_queue_family(), ForgottenEngine::VulkanContext::get_allocator(), width,
		height, global_min_image_count);
}

static void cleanup_vulkan_window()
{
	ImGui_ImplVulkanH_DestroyWindow(ForgottenEngine::VulkanContext::get_instance(),
		ForgottenEngine::VulkanContext::get_device(), &global_main_window_data,
		ForgottenEngine::VulkanContext::get_allocator());
}

static void create_descriptor_pool()
{
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * ((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
	pool_info.poolSizeCount = (uint32_t)((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
	pool_info.pPoolSizes = pool_sizes;
	auto err = vkCreateDescriptorPool(ForgottenEngine::VulkanContext::get_device(), &pool_info,
		ForgottenEngine::VulkanContext::get_allocator(), &global_descriptor_pool);
	check_vk_result(err);
}

static void frame_render(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
	VkResult err;

	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	err = vkAcquireNextImageKHR(ForgottenEngine::VulkanContext::get_device(), wd->Swapchain, UINT64_MAX,
		image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		global_swap_chain_rebuild = true;
		return;
	}
	check_vk_result(err);

	s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % global_main_window_data.ImageCount;

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(ForgottenEngine::VulkanContext::get_device(), 1, &fd->Fence, VK_TRUE,
			UINT64_MAX); // wait indefinitely instead of periodically checking
		check_vk_result(err);

		err = vkResetFences(ForgottenEngine::VulkanContext::get_device(), 1, &fd->Fence);
		check_vk_result(err);
	}

	{
		// Free resources in queue
		for (auto& func : resource_free_queue[s_CurrentFrameIndex])
			func();
		resource_free_queue[s_CurrentFrameIndex].clear();
	}
	{
		// Free command buffers allocated by Application::GetCommandBuffer
		// These use global_main_window_data.FrameIndex and not s_CurrentFrameIndex because they're tied to the
		// swapchain image index
		auto& allocatedCommandBuffers = allocated_framebuffers[wd->FrameIndex];
		if (!allocatedCommandBuffers.empty()) {
			vkFreeCommandBuffers(ForgottenEngine::VulkanContext::get_device(), fd->CommandPool,
				(uint32_t)allocatedCommandBuffers.size(), allocatedCommandBuffers.data());
			allocatedCommandBuffers.clear();
		}

		err = vkResetCommandPool(ForgottenEngine::VulkanContext::get_device(), fd->CommandPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	// Submit command buffer
	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(ForgottenEngine::VulkanContext::get_queue(), 1, &info, fd->Fence);
		check_vk_result(err);
	}
}

static void frame_present(ImGui_ImplVulkanH_Window* wd)
{
	if (global_swap_chain_rebuild)
		return;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(ForgottenEngine::VulkanContext::get_queue(), &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		global_swap_chain_rebuild = true;
		return;
	}
	check_vk_result(err);
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount; // Now we can use the next set of semaphores
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

namespace Forgotten {

Application::Application(ApplicationProperties specification)
	: spec(std::move(specification))
{
	construct();
}

Application::~Application() { destruct(); }

void Application::construct()
{
	ForgottenEngine::VulkanContext::the();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_handle = glfwCreateWindow(spec.width, spec.height, spec.name.c_str(), nullptr, nullptr);

	// Create Window Surface
	VkSurfaceKHR surface;
	VkResult err = glfwCreateWindowSurface(ForgottenEngine::VulkanContext::get_instance(), window_handle,
		ForgottenEngine::VulkanContext::get_allocator(), &surface);
	check_vk_result(err);

	// Create Framebuffers
	int w, h;
	glfwGetFramebufferSize(window_handle, &w, &h);
	ImGui_ImplVulkanH_Window* wd = &global_main_window_data;
	setup_vulkan_window(wd, surface, w, h);

	allocated_framebuffers.resize(wd->ImageCount);
	resource_free_queue.resize(wd->ImageCount);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
	// io.ConfigViewportsNoAutoMerge = true;
	// io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to
	// regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	create_descriptor_pool();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(window_handle, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = ForgottenEngine::VulkanContext::get_instance();
	init_info.PhysicalDevice = ForgottenEngine::VulkanContext::get_physical_device();
	init_info.Device = ForgottenEngine::VulkanContext::get_device();
	init_info.QueueFamily = ForgottenEngine::VulkanContext::get_queue_family();
	init_info.Queue = ForgottenEngine::VulkanContext::get_queue();
	init_info.PipelineCache = global_pipeline_cache;
	init_info.DescriptorPool = global_descriptor_pool;
	init_info.Subpass = 0;
	init_info.MinImageCount = global_min_image_count;
	init_info.ImageCount = wd->ImageCount;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = ForgottenEngine::VulkanContext::get_allocator();
	init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

	// Load default font
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	ImFont* robotoFont
		= io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
	io.FontDefault = robotoFont;

	// Upload Fonts
	{
		// Use any command queue
		VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
		VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

		err = vkResetCommandPool(ForgottenEngine::VulkanContext::get_device(), command_pool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(command_buffer, &begin_info);
		check_vk_result(err);

		ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &command_buffer;
		err = vkEndCommandBuffer(command_buffer);
		check_vk_result(err);
		err = vkQueueSubmit(ForgottenEngine::VulkanContext::get_queue(), 1, &end_info, VK_NULL_HANDLE);
		check_vk_result(err);

		err = vkDeviceWaitIdle(ForgottenEngine::VulkanContext::get_device());
		check_vk_result(err);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}

void Application::destruct()
{
	for (auto& layer : layer_stack)
		layer->on_detach();

	layer_stack.clear();

	// Cleanup
	VkResult err = vkDeviceWaitIdle(ForgottenEngine::VulkanContext::get_device());
	check_vk_result(err);

	// Free resources in queue
	for (auto& queue : resource_free_queue) {
		for (auto& func : queue)
			func();
	}
	resource_free_queue.clear();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	cleanup_vulkan_window();

	glfwDestroyWindow(window_handle);
	glfwTerminate();

	is_application_running = false;
}

void Application::construct_and_run()
{
	running = true;

	ImGui_ImplVulkanH_Window* wd = &global_main_window_data;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	ImGuiIO& io = ImGui::GetIO();

	Timer timer;

	static constexpr auto MAX_INTERVAL_MA = 4.0f;
	static constexpr auto MAX_INTERVAL = 4000;
	float queue_aggregate{ 0 };
	std::queue<float> frame_lengths{};

	// Main loop
	while (!glfwWindowShouldClose(window_handle) && running) {
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use
		// your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on
		// those two flags.
		glfwPollEvents();

		resize_swap_chain(window_handle);

		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
			// because it would be confusing to have two docking targets within each others.
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
			if (menubar_callback)
				window_flags |= ImGuiWindowFlags_MenuBar;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

			// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
			// and handle the pass-thru hole, so we ask Begin() to not render a background.
			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
			// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
			// all active windows docked into it will lose their parent and become undocked.
			// We cannot preserve the docking relationship between an active window and an inactive docking,
			// otherwise any change of dockspace/settings would lead to windows being stuck in limbo and never
			// being visible.
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("DockSpace Demo", nullptr, window_flags);
			ImGui::PopStyleVar();

			ImGui::PopStyleVar(2);

			// Submit the DockSpace
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
				ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			if (menubar_callback) {
				if (ImGui::BeginMenuBar()) {
					menubar_callback();
					ImGui::EndMenuBar();
				}
			}

			for (auto& layer : layer_stack)
				layer->on_ui_render();

			ImGui::End();
		}

		// Rendering
		render_and_present(io, wd, clear_color);
		frame_time = timer.elapsed_millis();

		frame_lengths.push(frame_time);
		queue_aggregate += frame_time;

		while (queue_aggregate > MAX_INTERVAL) {
			auto old_frame = frame_lengths.front();
			frame_lengths.pop();
			queue_aggregate -= old_frame;
		}

		average_fps = static_cast<float>(frame_lengths.size()) / MAX_INTERVAL_MA;

		timer.reset();
	}
}

void Application::render_and_present(ImGuiIO& io, ImGui_ImplVulkanH_Window* window_data, const ImVec4& cc)
{
	ImGui::Render();
	ImDrawData* main_draw_data = ImGui::GetDrawData();
	const bool main_is_minimized
		= (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
	window_data->ClearValue.color.float32[0] = cc.x * cc.w;
	window_data->ClearValue.color.float32[1] = cc.y * cc.w;
	window_data->ClearValue.color.float32[2] = cc.z * cc.w;
	window_data->ClearValue.color.float32[3] = cc.w;
	if (!main_is_minimized)
		frame_render(window_data, main_draw_data);

	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	// Present Main Platform Window
	if (!main_is_minimized)
		frame_present(window_data);
}

void Application::resize_swap_chain(GLFWwindow* wh)
{
	// Resize swap chain?
	if (global_swap_chain_rebuild) {
		int width, height;
		glfwGetFramebufferSize(wh, &width, &height);
		if (width > 0 && height > 0) {
			ImGui_ImplVulkan_SetMinImageCount(global_min_image_count);
			ImGui_ImplVulkanH_CreateOrResizeWindow(ForgottenEngine::VulkanContext::get_instance(),
				ForgottenEngine::VulkanContext::get_physical_device(),
				ForgottenEngine::VulkanContext::get_device(), &global_main_window_data,
				ForgottenEngine::VulkanContext::get_queue_family(),
				ForgottenEngine::VulkanContext::get_allocator(), width, height, global_min_image_count);
			global_main_window_data.FrameIndex = 0;

			// Clear allocated command buffers from here since entire pool is destroyed
			allocated_framebuffers.clear();
			allocated_framebuffers.resize(global_main_window_data.ImageCount);

			global_swap_chain_rebuild = false;
		}
	}
}

void Application::close() { running = false; }

VkCommandBuffer Application::get_command_buffer()
{
	ImGui_ImplVulkanH_Window* wd = &global_main_window_data;

	// Use any command queue
	VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = command_pool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer& command_buffer = allocated_framebuffers[wd->FrameIndex].emplace_back();
	auto err = vkAllocateCommandBuffers(
		ForgottenEngine::VulkanContext::get_device(), &cmdBufAllocateInfo, &command_buffer);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	err = vkBeginCommandBuffer(command_buffer, &begin_info);
	check_vk_result(err);

	return command_buffer;
}

void Application::flush_command_buffer(VkCommandBuffer commandBuffer)
{
	const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &commandBuffer;
	auto err = vkEndCommandBuffer(commandBuffer);
	check_vk_result(err);

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	VkFence fence;
	err = vkCreateFence(ForgottenEngine::VulkanContext::get_device(), &fenceCreateInfo, nullptr, &fence);
	check_vk_result(err);

	err = vkQueueSubmit(ForgottenEngine::VulkanContext::get_queue(), 1, &end_info, fence);
	check_vk_result(err);

	err = vkWaitForFences(ForgottenEngine::VulkanContext::get_device(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	check_vk_result(err);

	vkDestroyFence(ForgottenEngine::VulkanContext::get_device(), fence, nullptr);
}

void Application::submit_resource_free(std::function<void()>&& func)
{
	resource_free_queue[s_CurrentFrameIndex].emplace_back(func);
}

float Application::get_frame_time() { return frame_time; }

float Application::get_average_fps() { return average_fps; }

}
