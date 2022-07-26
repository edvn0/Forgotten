//
// Created by Edwin Carlsson on 2022-07-09.
//

#include "fg_pch.hpp"

#include "imgui/ImGuiLayer.hpp"

#include "render/Renderer.hpp"

#include "Application.hpp"
#include "Input.hpp"

#include "GLFW/glfw3.h"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

static std::vector<VkCommandBuffer> imgui_command_buffers;

void ImGuiLayer::on_attach()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

	auto* instance = this;
	Renderer::submit([instance]() {
		Application& app = Application::the();
		auto* window = static_cast<GLFWwindow*>(app.get_window().get_natively());

		auto vulkanContext = VulkanContext::get();
		auto device = vulkanContext->get_device();

		VkDescriptorPool descriptorPool;

		// Create Descriptor Pool
		VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 }, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 100 * IM_ARRAYSIZE(pool_sizes);
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		VK_CHECK(vkCreateDescriptorPool(device->get_vulkan_device(), &pool_info, nullptr, &descriptorPool));

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = VulkanContext::get_instance();
		init_info.PhysicalDevice = device->get_physical_device()->get_vulkan_physical_device();
		init_info.Device = device->get_vulkan_device();
		init_info.QueueFamily = device->get_physical_device()->get_queue_family_indices().graphics;
		init_info.Queue = device->get_graphics_queue();
		init_info.PipelineCache = nullptr;
		init_info.DescriptorPool = descriptorPool;
		init_info.Allocator = nullptr;
		init_info.MinImageCount = 2;
		auto& swapchain = Application::the().get_window().get_swapchain();
		init_info.ImageCount = swapchain.get_image_count();
		ImGui_ImplVulkan_Init(&init_info, swapchain.get_render_pass());

		// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
		// ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among
		// multiple.
		// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your
		// application (e.g. use an assertion, or display an error and quit).
		// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
		// ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
		// - Read 'docs/FONTS.md' for more instructions and details.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a
		// double backslash \\ !
		// io.Fonts->AddFontDefault();
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
		// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL,
		// io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

		// Upload Fonts
		{
			// Use any command queue

			VkCommandBuffer commandBuffer = vulkanContext->get_device()->get_command_buffer(true);
			ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
			vulkanContext->get_device()->flush_command_buffer(commandBuffer);

			VK_CHECK(vkDeviceWaitIdle(device->get_vulkan_device()));
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}

		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
		imgui_command_buffers.resize(frames_in_flight);
		for (uint32_t i = 0; i < frames_in_flight; i++)
			imgui_command_buffers[i] = VulkanContext::get_current_device()->get_secondary_buffer();
	});
}

void ImGuiLayer::on_detach()
{
	Renderer::submit([]() {
		auto device = VulkanContext::get_current_device();

		VK_CHECK(vkDeviceWaitIdle(device->get_vulkan_device()));
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	});
}

void ImGuiLayer::on_update(const TimeStep& step)
{
	if (Input::key(Key::Escape)) {
		Application::the().exit();
	}
}

void ImGuiLayer::on_event(Event& e)
{
	if (block) {
		auto& io = ImGui::GetIO();
		e.handled |= e.is_in_category(EventCategoryMouse) & io.WantCaptureMouse;
		e.handled |= e.is_in_category(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}
}

void ImGuiLayer::end()
{
	ImGui::Render();

	auto& swapchain = Application::the().get_window().get_swapchain();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	uint32_t width = swapchain.get_width();
	uint32_t height = swapchain.get_height();

	uint32_t commandBufferIndex = swapchain.get_current_buffer_index();

	VkCommandBufferBeginInfo drawCmdBufInfo = {};
	drawCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	drawCmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	drawCmdBufInfo.pNext = nullptr;

	VkCommandBuffer drawCommandBuffer = swapchain.get_current_drawbuffer();
	VK_CHECK(vkBeginCommandBuffer(drawCommandBuffer, &drawCmdBufInfo));

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = swapchain.get_render_pass();
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2; // Color + depth
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.framebuffer = swapchain.get_current_framebuffer();

	vkCmdBeginRenderPass(drawCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = swapchain.get_render_pass();
	inheritanceInfo.framebuffer = swapchain.get_current_framebuffer();

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	cmdBufInfo.pInheritanceInfo = &inheritanceInfo;

	VK_CHECK(vkBeginCommandBuffer(imgui_command_buffers[commandBufferIndex], &cmdBufInfo));

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = (float)height;
	viewport.height = -(float)height;
	viewport.width = (float)width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(imgui_command_buffers[commandBufferIndex], 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(imgui_command_buffers[commandBufferIndex], 0, 1, &scissor);

	ImDrawData* main_draw_data = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(main_draw_data, imgui_command_buffers[commandBufferIndex]);

	VK_CHECK(vkEndCommandBuffer(imgui_command_buffers[commandBufferIndex]));

	std::vector<VkCommandBuffer> commandBuffers;
	commandBuffers.push_back(imgui_command_buffers[commandBufferIndex]);

	vkCmdExecuteCommands(drawCommandBuffer, uint32_t(commandBuffers.size()), commandBuffers.data());

	vkCmdEndRenderPass(drawCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(drawCommandBuffer));

	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

}
