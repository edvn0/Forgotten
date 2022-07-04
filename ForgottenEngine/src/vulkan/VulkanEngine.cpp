//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "vulkan/VulkanEngine.hpp"
#include "GLFW/glfw3.h"
#include "Mesh.hpp"
#include "VkBootstrap.h"
#include "glm/gtx/transform.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/VulkanInitializers.hpp"
#include "vulkan/VulkanPipelineBuilder.hpp"
#include "vulkan/VulkanPushConstant.hpp"

static VkDevice device() { return ForgottenEngine::VulkanContext::get_device(); }

static VkInstance instance() { return ForgottenEngine::VulkanContext::get_instance(); }

static VkPhysicalDevice physical_device() { return ForgottenEngine::VulkanContext::get_physical_device(); }

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

	if (!init_vma()) {
		return false;
	}

	if (!init_meshes()) {
		return false;
	}

	return true;
}

void VulkanEngine::run()
{
	while (!glfwWindowShouldClose(VulkanContext::get_window_handle())) {
		glfwWaitEvents(); // POLL EVENTS!

		if (glfwGetKey(VulkanContext::get_window_handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(VulkanContext::get_window_handle(), GLFW_TRUE);
		}

		render_and_present();
	}
}

bool VulkanEngine::init_vma()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = VulkanContext::get_physical_device();
	allocatorInfo.device = device();
	allocatorInfo.instance = instance();
	vmaCreateAllocator(&allocatorInfo, &allocator);

	return true;
}

bool VulkanEngine::init_swapchain()
{
	auto dev = device();
	auto physical_device = VulkanContext::get_physical_device();
	auto surface = VulkanContext::get_surface();
	vkb::SwapchainBuilder sc_builder{ physical_device, dev, surface };

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

	cleanup_queue.push_function([&swap = swapchain]() { vkDestroySwapchainKHR(device(), swap, nullptr); });

	return true;
}

bool VulkanEngine::init_commands()
{
	auto cpi = VI::command_pool_create_info(
		VulkanContext::get_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(device(), &cpi, nullptr, &command_pool));

	auto cai = VI::command_buffer_allocate_info(command_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(device(), &cai, &main_command_buffer));

	cleanup_queue.push_function([&cp = command_pool]() { vkDestroyCommandPool(device(), cp, nullptr); });

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

	VK_CHECK(vkCreateRenderPass(device(), &render_pass_info, nullptr, &render_pass));

	cleanup_queue.push_function([&rp = render_pass]() { vkDestroyRenderPass(device(), rp, nullptr); });

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
		VK_CHECK(vkCreateFramebuffer(device(), &fb_info, nullptr, &framebuffers[i]));

		cleanup_queue.push_function([&fb = framebuffers[i], &view = swapchain_images[i].view]() {
			vkDestroyFramebuffer(device(), fb, nullptr);
			vkDestroyImageView(device(), view, nullptr);
		});
	}

	return true;
}

bool VulkanEngine::init_sync_structures()
{
	auto fci = VI::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK(vkCreateFence(device(), &fci, nullptr, &render_fence));

	cleanup_queue.push_function([&f = render_fence]() { vkDestroyFence(device(), f, nullptr); });

	// for the semaphores we don't need any flags
	auto sci = VI::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(device(), &sci, nullptr, &present_sema));
	VK_CHECK(vkCreateSemaphore(device(), &sci, nullptr, &render_sema));

	cleanup_queue.push_function([&p = present_sema, &r = render_sema]() {
		vkDestroySemaphore(device(), r, nullptr);
		vkDestroySemaphore(device(), p, nullptr);
	});

	return true;
}

void VulkanEngine::render_and_present()
{
	VK_CHECK(vkWaitForFences(device(), 1, &render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(device(), 1, &render_fence));

	// request image from the swapchain, one second timeout
	uint32_t image_index;
	VK_CHECK(vkAcquireNextImageKHR(device(), swapchain, 1000000000, present_sema, nullptr, &image_index));

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

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

	// make a model view matrix for rendering the object
	// camera position
	glm::vec3 camPos = { 0.f, 0.f, -2.f };

	glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 800.f / 450.f, 0.1f, 200.0f);
	projection[1][1] *= -1;
	// model rotation
	glm::mat4 model = glm::rotate(
		glm::mat4{ 1.0f }, glm::radians(static_cast<float>(frame_number) * 0.4f), glm::vec3(0, 1, 0));

	// calculate final mesh matrix
	glm::mat4 mesh_matrix = projection * view * model;

	MeshPushConstants constants{ .data = { 0, 0, 0, 0 }, .render_matrix = mesh_matrix };

	// upload the matrix to the GPU via push constants
	vkCmdPushConstants(cmd, mesh_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

	// bind the mesh vertex buffer with offset 0
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &triangle_mesh.get_vertex_buffer().buffer, &offset);

	// we can now draw the mesh
	vkCmdDraw(cmd, triangle_mesh.get_vertices().size(), 1, 0, 0);

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
	triangle_coloured_vertex = Shader::create("shaders/triangle_shader_coloured.vert.spv");
	triangle_coloured_fragment = Shader::create("shaders/triangle_shader_coloured.frag.spv");
	mesh_vertex = Shader::create("shaders/tri_mesh.vert.spv");

	return true;
}

bool VulkanEngine::init_pipelines()
{
	{
		// Default Pipeline
		auto pipeline_layout_info = VI::Pipeline::pipeline_layout_create_info();
		VK_CHECK(vkCreatePipelineLayout(device(), &pipeline_layout_info, nullptr, &triangle_layout));
	}

	{
		// Push Constants in Pipeline Layout

		// we start from just the default empty pipeline layout info
		auto mesh_pipeline_layout_info = VI::Pipeline::pipeline_layout_create_info();

		// setup push constants
		VkPushConstantRange push_constant;
		// this push constant range starts at the beginning
		push_constant.offset = 0;
		// this push constant range takes up the size of a MeshPushConstants struct
		push_constant.size = sizeof(MeshPushConstants);
		// this push constant range is accessible only in the vertex shader
		push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
		mesh_pipeline_layout_info.pushConstantRangeCount = 1;
		VK_CHECK(vkCreatePipelineLayout(device(), &mesh_pipeline_layout_info, nullptr, &mesh_layout));
	}

	VulkanPipelineBuilder pipeline_builder;

	pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
		VK_SHADER_STAGE_VERTEX_BIT, triangle_vertex->get_module()));

	pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
		VK_SHADER_STAGE_FRAGMENT_BIT, triangle_fragment->get_module()));

	// vertex input controls how to read vertices from vertex buffers. We aren't using it yet
	auto vertex_description = Vertex::get_vertex_description();
	auto vii = VI::Pipeline::vertex_input_state_create_info();
	// connect the pipeline builder vertex input info to the one we get from Vertex
	vii.pVertexAttributeDescriptions = vertex_description.attributes.data();
	vii.vertexAttributeDescriptionCount = vertex_description.attributes.size();

	vii.pVertexBindingDescriptions = vertex_description.bindings.data();
	vii.vertexBindingDescriptionCount = vertex_description.bindings.size();

	pipeline_builder.with_vertex_input(vii);

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

	{
		// Normal Triangle Pipeline
		triangle_pipeline = pipeline_builder.build(render_pass);
	}
	{
		// Coloured Triangle Pipeline
		pipeline_builder.clear_shader_stages();

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_VERTEX_BIT, triangle_coloured_vertex->get_module()));

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_FRAGMENT_BIT, triangle_coloured_fragment->get_module()));

		coloured_triangle_pipeline = pipeline_builder.build(render_pass);
	}

	{
		// Mesh Pipeline
		pipeline_builder.clear_shader_stages();

		pipeline_builder.with_pipeline_layout(mesh_layout);

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_VERTEX_BIT, mesh_vertex->get_module()));

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_FRAGMENT_BIT, triangle_coloured_fragment->get_module()));

		mesh_pipeline = pipeline_builder.build(render_pass);
	}

	triangle_coloured_vertex->destroy();
	triangle_vertex->destroy();

	triangle_coloured_fragment->destroy();
	triangle_fragment->destroy();

	mesh_vertex->destroy();

	cleanup_queue.push_function([=]() {
		// destroy the 2 pipelines we have created
		vkDestroyPipeline(device(), coloured_triangle_pipeline, nullptr);
		vkDestroyPipeline(device(), triangle_pipeline, nullptr);
		vkDestroyPipeline(device(), mesh_pipeline, nullptr);

		// destroy the pipeline layout that they use
		vkDestroyPipelineLayout(device(), triangle_layout, nullptr);
	});

	return true;
}

bool VulkanEngine::init_meshes()
{
	auto& vertices = triangle_mesh.get_vertices();

	monkey_mesh = Mesh::create("models/monkey_smooth.obj");
	monkey_mesh->upload();

	vertices.resize(3);
	vertices[0].position = { 1.f, 1.f, 0.0f, 0.0f };
	vertices[1].position = { -1.f, 1.f, 0.0f, 0.0f };
	vertices[2].position = { 0.f, -1.f, 0.0f, 0.0f };

	// vertex colors, all green
	vertices[0].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green
	vertices[1].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green
	vertices[2].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green

	upload_mesh(triangle_mesh);

	return true;
}

bool VulkanEngine::upload_mesh(DynamicMesh& mesh)
{
	// allocate vertex buffer
	VkBufferCreateInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// this is the total size, in bytes, of the buffer we are allocating
	bi.size = mesh.size();
	// this buffer is going to be used as a Vertex Buffer
	bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	// let the VMA library know that this data should be writeable by CPU, but also readable by GPU
	VmaAllocationCreateInfo ai = {};
	ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	// allocate the buffer
	auto& vb = mesh.get_vertex_buffer();
	VK_CHECK(vmaCreateBuffer(allocator, &bi, &ai, &vb.buffer, &vb.allocation, nullptr));

	// add the destruction of triangle mesh buffer to the deletion queue
	cleanup_queue.push_function([=]() { vmaDestroyBuffer(allocator, vb.buffer, vb.allocation); });

	void* data;
	vmaMapMemory(allocator, vb.allocation, &data);

	memcpy(data, mesh.get_vertices().data(), mesh.size());

	vmaUnmapMemory(allocator, vb.allocation);
	return false;
}

void VulkanEngine::cleanup()
{
	// make sure the GPU has stopped doing its things
	vkWaitForFences(device(), 1, &render_fence, true, 1000000000);

	cleanup_queue.flush();

	vmaDestroyAllocator(allocator);

	vkDestroySurfaceKHR(instance(), VulkanContext::get_surface(), nullptr);

	vkDestroyDevice(device(), nullptr);
	vkDestroyInstance(instance(), nullptr);

	glfwDestroyWindow(VulkanContext::get_window_handle());
}

} // ForgottenEngine
