//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "vulkan/VulkanEngine.hpp"
#include "GLFW/glfw3.h"
#include "VkBootstrap.h"
#include "vulkan/VulkanInitializers.hpp"
#include "vulkan/VulkanPipelineBuilder.hpp"
#include <unistd.h>

namespace ForgottenEngine {

bool VulkanEngine::initialize()
{
	if (!init_swapchain()) {
		return false;
	}

	if (!init_commands()) {
		return false;
	}

	if (!init_default_renderpass()) {
		return false;
	}

	if (!init_framebuffers()) {
		return false;
	}

	if (!init_sync_structures()) {
		return false;
	}

	if (!init_shader()) {
		return false;
	}

	if (!init_pipelines()) {
		return false;
	}

	return true;
}

void VulkanEngine::run()
{
	while (!glfwWindowShouldClose(VulkanContext::get_window_handle())) {
		glfwPollEvents();

		render_and_present();
	}
}

bool VulkanEngine::init_swapchain()
{
	auto device = VulkanContext::get_device();
	auto physical_device = VulkanContext::get_physical_device();
	auto surface = VulkanContext::get_surface();
	vkb::SwapchainBuilder sc_builder{ physical_device, device, surface };

	const auto&& [w, h] = VulkanContext::get_framebuffer_size();
	vkb::Swapchain vkb_sc = sc_builder
								.use_default_format_selection()
								// use vsync present mode
								.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
								.set_desired_extent(spec.w, spec.h)
								.build()
								.value();

	window_extent = { spec.w, spec.h };

	// store swapchain and its related images
	swapchain = vkb_sc.swapchain;
	swapchain_image_format = vkb_sc.image_format;

	const auto& images = vkb_sc.get_images();
	const auto& views = vkb_sc.get_image_views();
	for (int i = 0; i < vkb_sc.image_count; i++) {
		swapchain_images.push_back({ images->at(i), views->at(i) });
	}

	return true;
}

bool VulkanEngine::init_commands()
{
	auto cpi = VI::command_pool_create_info(
		VulkanContext::get_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(VulkanContext::get_device(), &cpi, nullptr, &command_pool));

	auto cai = VI::command_buffer_allocate_info(command_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(VulkanContext::get_device(), &cai, &main_command_buffer));

	return true;
}

bool VulkanEngine::init_default_renderpass()
{
	// the renderpass will use this color attachment.
	VkAttachmentDescription color_attachment = {};
	// the attachment will have the format needed by the swapchain
	color_attachment.format = swapchain_image_format;
	// 1 sample, we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we Clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	// attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	// connect the color attachment to the info
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	// connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(VulkanContext::get_device(), &render_pass_info, nullptr, &render_pass));

	return true;
}

bool VulkanEngine::init_framebuffers()
{
	// create the framebuffers for the swapchain images. This will connect the render-pass to the images for
	// rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = render_pass;
	fb_info.attachmentCount = 1;
	fb_info.width = spec.w;
	fb_info.height = spec.h;
	fb_info.layers = 1;

	// grab how many images we have in the swapchain
	const uint32_t sc_image_count = swapchain_images.size();
	framebuffers = std::vector<VkFramebuffer>(sc_image_count);

	// create framebuffers for each of the swapchain image views
	for (int i = 0; i < sc_image_count; i++) {
		fb_info.pAttachments = &swapchain_images[i].view;
		VK_CHECK(vkCreateFramebuffer(VulkanContext::get_device(), &fb_info, nullptr, &framebuffers[i]));
	}

	return true;
}

bool VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fci = {};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.pNext = nullptr;

	// we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU
	// command (for the first frame)
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(VulkanContext::get_device(), &fci, nullptr, &render_fence));

	// for the semaphores we don't need any flags
	VkSemaphoreCreateInfo sci = {};
	sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	sci.pNext = nullptr;
	sci.flags = 0;

	VK_CHECK(vkCreateSemaphore(VulkanContext::get_device(), &sci, nullptr, &present_sema));
	VK_CHECK(vkCreateSemaphore(VulkanContext::get_device(), &sci, nullptr, &render_sema));

	return true;
}

void VulkanEngine::render_and_present()
{
	VK_CHECK(vkWaitForFences(VulkanContext::get_device(), 1, &render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(VulkanContext::get_device(), 1, &render_fence));

	// request image from the swapchain, one second timeout
	uint32_t image_index;
	VK_CHECK(vkAcquireNextImageKHR(
		VulkanContext::get_device(), swapchain, 1000000000, present_sema, nullptr, &image_index));

	VK_CHECK(vkResetCommandBuffer(main_command_buffer, 0));

	// naming it cmd for shorter writing
	VkCommandBuffer cmd = main_command_buffer;

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan
	// know that
	VkCommandBufferBeginInfo cbi = {};
	cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbi.pNext = nullptr;

	cbi.pInheritanceInfo = nullptr;
	cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));

	// make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clear_value;
	float flash = abs(sin(static_cast<float>(frame_number) / 120.f));
	clear_value.color = { { 0.0f, 0.0f, flash, 1.0f } };

	// start the main renderpass.
	// We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpi = {};
	rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpi.pNext = nullptr;

	rpi.renderPass = render_pass;
	rpi.renderArea.offset.x = 0;
	rpi.renderArea.offset.y = 0;
	rpi.renderArea.extent = window_extent;
	rpi.framebuffer = framebuffers[image_index];

	// connect clear values
	rpi.clearValueCount = 1;
	rpi.pClearValues = &clear_value;

	vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);

	// once we start adding rendering commands, they will go here
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRenderPass(cmd);
	// finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &wait_stage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &present_sema;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &render_sema;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(VulkanContext::get_queue(), 1, &submit, render_fence));

	VkPresentInfoKHR pi = {};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.pNext = nullptr;

	pi.pSwapchains = &swapchain;
	pi.swapchainCount = 1;

	pi.pWaitSemaphores = &render_sema;
	pi.waitSemaphoreCount = 1;

	pi.pImageIndices = &image_index;

	VK_CHECK(vkQueuePresentKHR(VulkanContext::get_queue(), &pi));

	// increase the number of frames drawn
	frame_number++;
}

bool VulkanEngine::init_shader()
{
	triangle_vertex = Shader::create("shaders/triangle_shader.vert.spv");
	triangle_fragment = Shader::create("shaders/triangle_shader.frag.spv");

	return true;
}

bool VulkanEngine::init_pipelines()
{
	auto pipeline_layout_info = VI::Pipeline::pipeline_layout_create_info();
	VK_CHECK(
		vkCreatePipelineLayout(VulkanContext::get_device(), &pipeline_layout_info, nullptr, &triangle_layout));

	VulkanPipelineBuilder pipeline_builder;

	pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
		VK_SHADER_STAGE_VERTEX_BIT, triangle_vertex->get_module()));

	pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
		VK_SHADER_STAGE_FRAGMENT_BIT, triangle_fragment->get_module()));

	// vertex input controls how to read vertices from vertex buffers. We aren't using it yet
	pipeline_builder.with_vertex_input(VI::Pipeline::vertex_input_state_create_info());

	// input assembly is the configuration for drawing triangle lists, strips, or individual points.
	// we are just going to draw triangle list
	pipeline_builder.with_input_assembly(
		VI::Pipeline::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));

	// build viewport and scissor from the swapchain extents
	VkViewport vp{};
	vp.x = 0.0f;
	vp.y = 0.0f;
	vp.width = (float)spec.w;
	vp.height = (float)spec.h;
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	pipeline_builder.with_viewport(vp);

	VkRect2D scissors{};
	scissors.offset = { 0, 0 };
	scissors.extent = { spec.w, spec.h };
	pipeline_builder.with_scissors(scissors);

	// configure the rasterizer to draw filled triangles
	pipeline_builder.with_rasterizer(VI::Pipeline::rasterization_state_create_info(VK_POLYGON_MODE_FILL));

	// we don't use multisampling, so just run the default one
	pipeline_builder.with_multisampling(VI::Pipeline::multisampling_state_create_info());

	// a single blend attachment with no blending and writing to RGBA
	pipeline_builder.with_color_blend(VI::Pipeline::color_blend_attachment_state());

	// use the triangle layout we created
	pipeline_builder.with_pipeline_layout(triangle_layout);

	// finally build the pipeline
	triangle_pipeline = pipeline_builder.build(render_pass);

	return true;
}

void VulkanEngine::cleanup()
{
	vkDestroyCommandPool(VulkanContext::get_device(), command_pool, nullptr);

	vkDestroySwapchainKHR(VulkanContext::get_device(), swapchain, nullptr);

	// destroy the main renderpass
	vkDestroyRenderPass(VulkanContext::get_device(), render_pass, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < framebuffers.size(); i++) {
		vkDestroyFramebuffer(VulkanContext::get_device(), framebuffers[i], nullptr);

		vkDestroyImageView(VulkanContext::get_device(), swapchain_images[i].view, nullptr);
	}
}

} // ForgottenEngine