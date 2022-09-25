//
// Created by Edwin Carlsson on 2022-07-09.
//

#include "fg_pch.hpp"

#include "imgui/ImGuiLayer.hpp"

#include "Application.hpp"
#include "GLFW/glfw3.h"
#include "Input.hpp"
#include "render/Renderer.hpp"
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

		Renderer::submit([]() {
			Application& app = Application::the();
			auto* window = static_cast<GLFWwindow*>(app.get_window().get_natively());

			auto vulkan_context = VulkanContext::get();
			auto device = vulkan_context->get_device();

			VkDescriptorPool imgui_descriptor_pool;

			// Create Descriptor Pool
			VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 100 * IM_ARRAYSIZE(pool_sizes);
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			vk_check(vkCreateDescriptorPool(device->get_vulkan_device(), &pool_info, nullptr, &imgui_descriptor_pool));

			// Setup Platform/Renderer bindings
			ImGui_ImplGlfw_InitForVulkan(window, true);
			ImGui_ImplVulkan_InitInfo init_info = {};
			init_info.Instance = VulkanContext::get_instance();
			init_info.PhysicalDevice = device->get_physical_device()->get_vulkan_physical_device();
			init_info.Device = device->get_vulkan_device();
			init_info.QueueFamily = device->get_physical_device()->get_queue_family_indices().graphics;
			init_info.Queue = device->get_graphics_queue();
			init_info.PipelineCache = nullptr;
			init_info.DescriptorPool = imgui_descriptor_pool;
			init_info.Allocator = nullptr;
			init_info.MinImageCount = 2;
			auto& swapchain = Application::the().get_window().get_swapchain();
			init_info.ImageCount = swapchain.get_image_count();
			ImGui_ImplVulkan_Init(&init_info, swapchain.get_render_pass());

			std::vector<OptionalPath> fonts = Assets::load_from_directory("fonts", false);
			for (const auto& font : fonts) {
				if (font) {
					ImGui::GetIO().Fonts->AddFontFromFileTTF((*font).string().c_str(), 12.0f);
				}
			}

			// Upload Fonts
			{
				VkCommandBuffer command_buffer = vulkan_context->get_device()->get_command_buffer(true);
				ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
				vulkan_context->get_device()->flush_command_buffer(command_buffer);

				vk_check(vkDeviceWaitIdle(device->get_vulkan_device()));
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

			vk_check(vkDeviceWaitIdle(device->get_vulkan_device()));
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
		auto& io = ImGui::GetIO();
		e.handled |= e.is_in_category(EventCategoryMouse) & io.WantCaptureMouse;
		e.handled |= e.is_in_category(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}

	void ImGuiLayer::begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::end()
	{
		ImGui::Render();

		static constexpr VkClearColorValue clear_colour = { 0.1f, 0.1f, 0.1f, 1.0f };

		auto& swapchain = Application::the().get_window().get_swapchain();

		std::array<VkClearValue, 2> clear_values {};
		clear_values[0].color = clear_colour;
		clear_values[1].depthStencil = { .depth = 1.0f, .stencil = 0 };

		uint32_t width = swapchain.get_width();
		uint32_t height = swapchain.get_height();

		uint32_t command_buffer_index = swapchain.get_current_buffer_index();

		VkCommandBufferBeginInfo cmd_bbi = {};
		cmd_bbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_bbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		cmd_bbi.pNext = nullptr;

		VkCommandBuffer draw_command_buffer = swapchain.get_current_drawbuffer();
		vk_check(vkBeginCommandBuffer(draw_command_buffer, &cmd_bbi));

		VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.pNext = nullptr;
		render_pass_begin_info.renderPass = swapchain.get_render_pass();
		render_pass_begin_info.renderArea.offset.x = 0;
		render_pass_begin_info.renderArea.offset.y = 0;
		render_pass_begin_info.renderArea.extent.width = width;
		render_pass_begin_info.renderArea.extent.height = height;
		render_pass_begin_info.clearValueCount = clear_values.size();
		render_pass_begin_info.pClearValues = clear_values.data();
		render_pass_begin_info.framebuffer = swapchain.get_current_framebuffer();

		vkCmdBeginRenderPass(draw_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		VkCommandBufferInheritanceInfo inheritance_info = {};
		inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance_info.renderPass = swapchain.get_render_pass();
		inheritance_info.framebuffer = swapchain.get_current_framebuffer();

		VkCommandBufferBeginInfo cbi = {};
		cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cbi.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		cbi.pInheritanceInfo = &inheritance_info;

		vk_check(vkBeginCommandBuffer(imgui_command_buffers[command_buffer_index], &cbi));

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float)height;
		viewport.height = (float)height;
		viewport.width = (float)width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(imgui_command_buffers[command_buffer_index], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(imgui_command_buffers[command_buffer_index], 0, 1, &scissor);

		ImDrawData* main_draw_data = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(main_draw_data, imgui_command_buffers[command_buffer_index]);

		vk_check(vkEndCommandBuffer(imgui_command_buffers[command_buffer_index]));

		std::vector<VkCommandBuffer> buffers;
		buffers.push_back(imgui_command_buffers[command_buffer_index]);

		vkCmdExecuteCommands(draw_command_buffer, uint32_t(buffers.size()), buffers.data());

		vkCmdEndRenderPass(draw_command_buffer);

		vk_check(vkEndCommandBuffer(draw_command_buffer));

		ImGuiIO& io = ImGui::GetIO();
		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

} // namespace ForgottenEngine
