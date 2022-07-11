//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "vulkan/VulkanEngine.hpp"

#include "Clock.hpp"
#include "Image.hpp"
#include "Input.hpp"
#include "Mesh.hpp"

#include "vulkan/VulkanInitializers.hpp"
#include "vulkan/VulkanPipelineBuilder.hpp"
#include "vulkan/VulkanPushConstant.hpp"
#include "vulkan/VulkanUBO.hpp"

#include "GLFW/glfw3.h"
#include "MemoryMapper.hpp"
#include "TimeStep.hpp"
#include "VkBootstrap.h"
#include "imgui/ImGui.hpp"
#include "imgui_internal.h"
#include "vk_mem_alloc.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

static VkDevice device() { return ForgottenEngine::VulkanContext::get_device(); }

static VkInstance instance() { return ForgottenEngine::VulkanContext::get_instance(); }

static VkPhysicalDevice physical_device() { return ForgottenEngine::VulkanContext::get_physical_device(); }

static constexpr int MAX_OBJECTS = 10'000;

namespace ForgottenEngine {

bool VulkanEngine::initialize()
{
	init_vma();

	init_swapchain();

	init_commands();

	init_default_renderpass();

	init_framebuffers();

	init_sync_structures();

	init_descriptors();

	init_shader();

	init_pipelines();

	init_meshes();

	init_scene();

	load_imgui(cleanup_queue, upload_context, render_pass);

	return true;
}

void VulkanEngine::update(const TimeStep& step) { render_and_present(step); }

void VulkanEngine::init_vma()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = VulkanContext::get_physical_device();
	allocatorInfo.device = device();
	allocatorInfo.instance = instance();
	vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanEngine::init_swapchain()
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
								.set_desired_extent(spec.width, spec.height)
								.build()
								.value();

	window_extent = { spec.width, spec.height };

	// store swapchain and its related images
	swapchain = vkb_sc.swapchain;
	swapchain_image_format = vkb_sc.image_format;

	const auto& images = vkb_sc.get_images();
	const auto& views = vkb_sc.get_image_views();
	for (int i = 0; i < vkb_sc.image_count; i++) {
		swapchain_images.push_back({ images->at(i), views->at(i) });
	}

	VkExtent3D depth_extent = { spec.width, spec.height, 1 };

	// hardcoding the depth format to 32 bit float
	depth_format = VK_FORMAT_D32_SFLOAT;

	// the depth image will be an image with the format we selected and Depth Attachment usage flag
	auto di
		= VI::Image::image_create_info(depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_extent);

	// for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dic = {};
	dic.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dic.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(allocator, &di, &dic, &depth_image.image, &depth_image.allocation, nullptr);

	// build an image-view for the depth image to use for rendering
	VkImageViewCreateInfo depth_image_view_create_info
		= VI::Image::image_view_create_info(depth_format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device(), &depth_image_view_create_info, nullptr, &depth_view));

	// add to deletion queues
	cleanup_queue.push_function([=]() {
		vkDestroyImageView(device(), depth_view, nullptr);
		vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);
	});

	cleanup_queue.push_function([&swap = swapchain]() { vkDestroySwapchainKHR(device(), swap, nullptr); });
}

void VulkanEngine::init_commands()
{
	// Immediate-submit context
	auto ucpi = VI::command_pool_create_info(VulkanContext::get_queue_family());
	// create pool for upload context
	VK_CHECK(vkCreateCommandPool(device(), &ucpi, nullptr, &upload_context.upload_pool));
	cleanup_queue.push_function([=]() { vkDestroyCommandPool(device(), upload_context.upload_pool, nullptr); });
	// allocate the default command buffer that we will use for the instant commands
	VkCommandBufferAllocateInfo cai_immediate = VI::command_buffer_allocate_info(upload_context.upload_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(device(), &cai_immediate, &upload_context.upload_command_buffer));

	// End immediate-submit context

	auto cpi = VI::command_pool_create_info(
		VulkanContext::get_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		auto& f = frames_in_flight[i];
		VK_CHECK(vkCreateCommandPool(device(), &cpi, nullptr, &f.command_pool));

		auto cai_engine = VI::command_buffer_allocate_info(f.command_pool, 1);
		VK_CHECK(vkAllocateCommandBuffers(device(), &cai_engine, &f.main_command_buffer));

		cleanup_queue.push_function([cp = f.command_pool]() { vkDestroyCommandPool(device(), cp, nullptr); });
	}
}

void VulkanEngine::init_default_renderpass()
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

	VkAttachmentDescription depth_attachment = {};
	// Depth attachment
	depth_attachment.flags = 0;
	depth_attachment.format = depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	const std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	// connect the color attachment to the info
	render_pass_info.attachmentCount = attachments.size();
	render_pass_info.pAttachments = attachments.data();
	// connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = 0;
	depth_dependency.srcStageMask
		= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.srcAccessMask = 0;
	depth_dependency.dstStageMask
		= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	const std::array<VkSubpassDependency, 2> dependencies = { dependency, depth_dependency };
	render_pass_info.dependencyCount = dependencies.size();
	render_pass_info.pDependencies = dependencies.data();

	VK_CHECK(vkCreateRenderPass(device(), &render_pass_info, nullptr, &render_pass));

	cleanup_queue.push_function([&rp = render_pass]() { vkDestroyRenderPass(device(), rp, nullptr); });
}

void VulkanEngine::init_framebuffers()
{
	// create the framebuffers for the swapchain images. This will connect the render-pass to the images for
	// rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = render_pass;
	fb_info.attachmentCount = 1;
	fb_info.width = spec.width;
	fb_info.height = spec.height;
	fb_info.layers = 1;

	// grab how many images we have in the swapchain
	const uint32_t sc_image_count = swapchain_images.size();
	framebuffers = std::vector<VkFramebuffer>(sc_image_count);

	// create framebuffers for each of the swapchain image views
	for (int i = 0; i < sc_image_count; i++) {
		VkImageView attachments[2];
		attachments[0] = swapchain_images[i].view;
		attachments[1] = depth_view;

		fb_info.attachmentCount = 2;
		fb_info.pAttachments = attachments;

		VK_CHECK(vkCreateFramebuffer(device(), &fb_info, nullptr, &framebuffers[i]));

		cleanup_queue.push_function([&fb = framebuffers[i], &view = swapchain_images[i].view]() {
			vkDestroyFramebuffer(device(), fb, nullptr);
			vkDestroyImageView(device(), view, nullptr);
		});
	}
}

void VulkanEngine::init_sync_structures()
{
	// Create immediate-submit context
	auto ufi = VI::fence_create_info();
	VK_CHECK(vkCreateFence(device(), &ufi, nullptr, &upload_context.upload_fence));

	cleanup_queue.push_function([=]() { vkDestroyFence(device(), upload_context.upload_fence, nullptr); });

	auto fci = VI::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	auto sci = VI::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(device(), &fci, nullptr, &frames_in_flight[i].render_fence));

		// enqueue the destruction of the fence
		cleanup_queue.push_function(
			[=]() { vkDestroyFence(device(), frames_in_flight[i].render_fence, nullptr); });

		VK_CHECK(vkCreateSemaphore(device(), &sci, nullptr, &frames_in_flight[i].present_sema));
		VK_CHECK(vkCreateSemaphore(device(), &sci, nullptr, &frames_in_flight[i].render_sema));

		// enqueue the destruction of semaphores
		cleanup_queue.push_function([=]() {
			vkDestroySemaphore(device(), frames_in_flight[i].present_sema, nullptr);
			vkDestroySemaphore(device(), frames_in_flight[i].render_sema, nullptr);
		});
	}
}

void VulkanEngine::render_and_present(const TimeStep& step)
{
	ImGui::Render();
	VK_CHECK(vkWaitForFences(device(), 1, &frame().render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(device(), 1, &frame().render_fence));

	// request image from the swapchain, one second timeout
	uint32_t image_index;
	VK_CHECK(vkAcquireNextImageKHR(device(), swapchain, 1000000000, frame().present_sema, nullptr, &image_index));
	VK_CHECK(vkResetCommandBuffer(frame().main_command_buffer, 0));

	// naming it cmd for shorter writing
	VkCommandBuffer cmd = frame().main_command_buffer;

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan
	// know that
	VkCommandBufferBeginInfo cbi = {};
	cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbi.pNext = nullptr;

	cbi.pInheritanceInfo = nullptr;
	cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));

	VkClearValue clear_value;
	clear_value.color = { 0.1f, 0.1f, 0.1f, 1.0f };

	VkClearValue depth_clear;
	depth_clear.depthStencil.depth = 1.f;

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

	const std::array<VkClearValue, 2> clear_values = { clear_value, depth_clear };
	rpi.clearValueCount = clear_values.size();
	rpi.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);

	draw_renderables(cmd);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRenderPass(cmd);
	// finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &wait_stage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &frame().present_sema;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &frame().render_sema;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	VK_CHECK(vkQueueSubmit(VulkanContext::get_queue(), 1, &submit, frame().render_fence));

	VkPresentInfoKHR pi = {};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.pNext = nullptr;

	pi.pSwapchains = &swapchain;
	pi.swapchainCount = 1;

	pi.pWaitSemaphores = &frame().render_sema;
	pi.waitSemaphoreCount = 1;

	pi.pImageIndices = &image_index;

	VK_CHECK(vkQueuePresentKHR(VulkanContext::get_queue(), &pi));

	// increase the number of frames drawn
	frame_number++;
}

void VulkanEngine::init_shader() { }

void VulkanEngine::init_pipelines()
{
	auto mesh_vertex = Shader::create("shaders/tri_mesh.vert.spv");
	auto mesh_fragment = Shader::create("shaders/default_lighting.frag.spv");
	auto mesh_textured = Shader::create("shaders/textured_lighting.frag.spv");

	VkPipeline mesh_pipeline{ nullptr };
	VkPipeline textured_pipeline{ nullptr };

	// we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = VI::Pipeline::pipeline_layout_create_info();
	VkPushConstantRange push_constant;
	push_constant.offset = 0;
	push_constant.size = sizeof(MeshPushConstants);
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;
	VkDescriptorSetLayout set_layouts[] = { global_set_layout, object_set_layout };

	mesh_pipeline_layout_info.setLayoutCount = 2;
	mesh_pipeline_layout_info.pSetLayouts = set_layouts;

	VkPipelineLayout mesh_layout;
	VK_CHECK(vkCreatePipelineLayout(device(), &mesh_pipeline_layout_info, nullptr, &mesh_layout));

	// we start from  the normal mesh layout
	VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;

	std::array<VkDescriptorSetLayout, 3> texture_set_layout_info
		= { global_set_layout, object_set_layout, texture_set_layout };

	textured_pipeline_layout_info.setLayoutCount = texture_set_layout_info.size();
	textured_pipeline_layout_info.pSetLayouts = texture_set_layout_info.data();

	VkPipelineLayout textured_layout;
	VK_CHECK(vkCreatePipelineLayout(device(), &textured_pipeline_layout_info, nullptr, &textured_layout));

	VulkanPipelineBuilder pipeline_builder;

	auto vertex_description = Vertex::get_vertex_description();
	auto vii = VI::Pipeline::vertex_input_state_create_info();

	vii.pVertexAttributeDescriptions = vertex_description.attributes.data();
	vii.vertexAttributeDescriptionCount = vertex_description.attributes.size();

	vii.pVertexBindingDescriptions = vertex_description.bindings.data();
	vii.vertexBindingDescriptionCount = vertex_description.bindings.size();

	pipeline_builder.with_vertex_input(vii);

	pipeline_builder.with_input_assembly(
		VI::Pipeline::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));

	// build viewport and scissor from the swapchain extents
	pipeline_builder.with_viewport({
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(spec.width),
		.height = static_cast<float>(spec.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	});

	pipeline_builder.with_scissors({ .offset = { 0, 0 }, .extent = { spec.width, spec.height } });

	pipeline_builder.with_rasterizer(VI::Pipeline::rasterization_state_create_info(VK_POLYGON_MODE_FILL));

	pipeline_builder.with_multisampling(VI::Pipeline::multisampling_state_create_info());

	pipeline_builder.with_color_blend(VI::Pipeline::color_blend_attachment_state());

	pipeline_builder.with_depth_stencil(
		VI::Pipeline::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL));

	// use the triangle layout we created
	{
		pipeline_builder.with_pipeline_layout(mesh_layout);
		// Mesh Pipeline
		pipeline_builder.clear_shader_stages();

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_VERTEX_BIT, mesh_vertex->get_module()));

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_FRAGMENT_BIT, mesh_fragment->get_module()));

		mesh_pipeline = pipeline_builder.build(render_pass);
		library.add_material("default", { .pipeline = mesh_pipeline, .layout = mesh_layout });
	}

	{
		// Textured Pipeline
		pipeline_builder.clear_shader_stages();

		pipeline_builder.with_pipeline_layout(textured_layout);

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_VERTEX_BIT, mesh_vertex->get_module()));

		pipeline_builder.with_stage(VI::Pipeline::pipeline_shader_stage_create_info(
			VK_SHADER_STAGE_FRAGMENT_BIT, mesh_textured->get_module()));

		textured_pipeline = pipeline_builder.build(render_pass);
	}

	library.add_material("textured", { .pipeline = textured_pipeline, .layout = textured_layout });

	mesh_vertex->destroy();
	mesh_fragment->destroy();
	mesh_textured->destroy();

	cleanup_queue.push_function([=]() {
		// destroy the 2 pipelines we have created
		vkDestroyPipeline(device(), mesh_pipeline, nullptr);
		vkDestroyPipeline(device(), textured_pipeline, nullptr);

		// destroy the pipeline layout that they use
		vkDestroyPipelineLayout(device(), mesh_layout, nullptr);
		vkDestroyPipelineLayout(device(), textured_layout, nullptr);
	});
}

void VulkanEngine::init_meshes()
{
	auto m = Mesh::create("models/monkey_smooth.obj");
	m->upload(allocator, cleanup_queue, upload_context);
	library.add_mesh("monkey_smooth", std::move(m));

	auto sponza = Mesh::create("models/sponza.obj");
	sponza->upload(allocator, cleanup_queue, upload_context);
	library.add_mesh("sponza", std::move(sponza));

	auto empire = Mesh::create("models/lost_empire.obj");
	empire->upload(allocator, cleanup_queue, upload_context);
	library.add_mesh("empire", std::move(empire));

	auto empire_tex = Image::create("textures/lost_empire.png");
	empire_tex->upload(allocator, cleanup_queue, upload_context);
	library.add_texture("lost_empire", std::move(empire_tex));

	auto w = Image::create("textures/lion.png");
	w->upload(allocator, cleanup_queue, upload_context);

	std::vector<Vertex> vertices;
	vertices.resize(3);
	vertices[0].position = { 0.f, -1.f, 0.0f, 1.0f };
	vertices[1].position = { -1.f, 1.f, 0.0f, 1.0f };
	vertices[2].position = { 1.f, 1.f, 0.0f, 1.0f };

	// Dunno
	vertices[0].normal = { 0.f, 1.f, 0.0f, 1.0f };
	vertices[1].normal = { 0.f, 1.f, 0.0f, 1.0f };
	vertices[2].normal = { 0.f, 1.f, 0.0f, 1.0f };

	// vertex colors, all green
	vertices[0].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green
	vertices[1].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green
	vertices[2].color = { 0.f, 1.f, 0.0f, 1.0f }; // pure green

	std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

	auto triangle_mesh = Mesh::create(vertices, indices);
	triangle_mesh->upload(allocator, cleanup_queue, upload_context);
	library.add_mesh("triangle", std::move(triangle_mesh));
}

void VulkanEngine::init_descriptors()
{
	// Create Descriptor Pool
	std::vector<VkDescriptorPoolSize> sizes = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 } };
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 10;
	pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
	pool_info.pPoolSizes = sizes.data();
	vkCreateDescriptorPool(device(), &pool_info, nullptr, &descriptor_pool);

	// information about the binding.
	auto camera_bind = VI::Descriptor::descriptor_set_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	auto scene_bind = VI::Descriptor::descriptor_set_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { camera_bind, scene_bind };
	VkDescriptorSetLayoutCreateInfo set_global_info = {};
	set_global_info.bindingCount = bindings.size();
	set_global_info.flags = 0;
	set_global_info.pNext = nullptr;
	set_global_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set_global_info.pBindings = bindings.data();
	vkCreateDescriptorSetLayout(device(), &set_global_info, nullptr, &global_set_layout);

	auto object_bind = VI::Descriptor::descriptor_set_layout_binding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutCreateInfo set_object_info = {};
	set_object_info.bindingCount = 1;
	set_object_info.flags = 0;
	set_object_info.pNext = nullptr;
	set_object_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set_object_info.pBindings = &object_bind;
	vkCreateDescriptorSetLayout(device(), &set_object_info, nullptr, &object_set_layout);

	VkDescriptorSetLayoutBinding texture_bind = VI::Descriptor::descriptor_set_layout_binding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	VkDescriptorSetLayoutCreateInfo set_texture_info = {};
	set_texture_info.bindingCount = 1;
	set_texture_info.flags = 0;
	set_texture_info.pNext = nullptr;
	set_texture_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set_texture_info.pBindings = &texture_bind;

	vkCreateDescriptorSetLayout(device(), &set_texture_info, nullptr, &texture_set_layout);

	const size_t buffer_size = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(SceneUBO));
	VulkanBuffer scene_buffer{ allocator };
	scene_buffer.upload(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	scene_parameter_buffer = scene_buffer.get_buffer();

	for (int frame = 0; frame < FRAME_OVERLAP; frame++) {
		VulkanBuffer camera_buffer{ allocator };
		camera_buffer.upload(sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		frames_in_flight[frame].camera_buffer = camera_buffer.get_buffer();

		VulkanBuffer object_buffer{ allocator };
		object_buffer.upload(
			sizeof(ObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		frames_in_flight[frame].object_buffer = object_buffer.get_buffer();

		VkDescriptorSetAllocateInfo alloc_global = {};
		alloc_global.pNext = nullptr;
		alloc_global.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_global.descriptorPool = descriptor_pool;
		alloc_global.descriptorSetCount = 1;
		alloc_global.pSetLayouts = &global_set_layout;
		vkAllocateDescriptorSets(device(), &alloc_global, &frames_in_flight[frame].global_descriptor);

		VkDescriptorSetAllocateInfo alloc_object = {};
		alloc_object.pNext = nullptr;
		alloc_object.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_object.descriptorPool = descriptor_pool;
		alloc_object.descriptorSetCount = 1;
		alloc_object.pSetLayouts = &object_set_layout;
		vkAllocateDescriptorSets(device(), &alloc_object, &frames_in_flight[frame].object_descriptor);

		VkDescriptorBufferInfo camera_info;
		camera_info.buffer = frames_in_flight[frame].camera_buffer.buffer;
		camera_info.offset = 0;
		camera_info.range = sizeof(CameraUBO);

		VkDescriptorBufferInfo scene_info;
		scene_info.buffer = scene_parameter_buffer.buffer;
		scene_info.offset = 0;
		scene_info.range = sizeof(SceneUBO);

		VkDescriptorBufferInfo object_info;
		object_info.buffer = frames_in_flight[frame].object_buffer.buffer;
		object_info.offset = 0;
		object_info.range = sizeof(ObjectData) * MAX_OBJECTS;

		auto camera_write = VI::Descriptor::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames_in_flight[frame].global_descriptor, &camera_info, 0);

		auto scene_write = VI::Descriptor::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, frames_in_flight[frame].global_descriptor, &scene_info, 1);

		auto object_write = VI::Descriptor::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frames_in_flight[frame].object_descriptor, &object_info, 0);

		std::array<VkWriteDescriptorSet, 3> set_writes = { camera_write, scene_write, object_write };

		vkUpdateDescriptorSets(device(), set_writes.size(), set_writes.data(), 0, nullptr);
	}

	cleanup_queue.push_function([&]() {
		vmaDestroyBuffer(allocator, scene_parameter_buffer.buffer, scene_parameter_buffer.allocation);

		vkDestroyDescriptorSetLayout(device(), object_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device(), global_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device(), texture_set_layout, nullptr);

		vkDestroyDescriptorPool(device(), descriptor_pool, nullptr);

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vmaDestroyBuffer(
				allocator, frames_in_flight[i].camera_buffer.buffer, frames_in_flight[i].camera_buffer.allocation);

			vmaDestroyBuffer(
				allocator, frames_in_flight[i].object_buffer.buffer, frames_in_flight[i].object_buffer.allocation);
		}
	});
};

void VulkanEngine::init_scene()
{
	VulkanRenderObject monkey;
	monkey.mesh = library.mesh("monkey_smooth");
	monkey.material = library.material("default");
	monkey.transform = glm::scale(glm::mat4{ 1.0 }, glm::vec3(3, 3, 3)) * glm::translate(glm::vec3(0, 2, 0));

	VulkanRenderObject sponza;
	sponza.mesh = library.mesh("sponza");
	sponza.material = library.material("default");
	sponza.transform
		= glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.1, 0.1, 0.1)) * glm::translate(glm::vec3{ 5, 100, 0 });

	auto textured_mat = library.material("textured");
	auto sampler_create_info = VI::Image::sampler_create_info(VK_FILTER_NEAREST);
	VkSampler block_sampler;
	vkCreateSampler(device(), &sampler_create_info, nullptr, &block_sampler);
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.pNext = nullptr;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &texture_set_layout;
	vkAllocateDescriptorSets(device(), &alloc_info, &textured_mat->texture_set);

	// write to the descriptor set so that it points to our empire_diffuse texture
	VkDescriptorImageInfo image_buffer_info;
	image_buffer_info.sampler = block_sampler;
	image_buffer_info.imageView = library.texture("lost_empire")->view();
	image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet write_ds = VI::Image::write_descriptor_image(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textured_mat->texture_set, &image_buffer_info, 0);
	vkUpdateDescriptorSets(device(), 1, &write_ds, 0, nullptr);

	VulkanRenderObject map;
	map.mesh = library.mesh("empire");
	map.material = textured_mat;
	map.transform = glm::translate(glm::vec3{ 5, -10, 0 });

	// renderables.push_back(sponza);
	renderables.push_back(monkey);
	renderables.push_back(map);
}

void VulkanEngine::draw_renderables(VkCommandBuffer cmd)
{
	static glm::vec3 cam_pos = { 0.f, -6.f, -10.f };
	static glm::vec3 cam_rot = { 0, 0, 0 };
	static bool renderables_are_sorted = false;
	static size_t last_renderable_size = 0;
	static constexpr auto move_speed = 0.03f;
	static constexpr auto rotate_speed = 0.005f;

	if (Input::key(Key::W)) {
		cam_pos.z += move_speed;
	}
	if (Input::key(Key::S)) {
		cam_pos.z -= move_speed;
	}

	if (Input::key(Key::Q)) {
		cam_rot.x += rotate_speed;
	}
	if (Input::key(Key::E)) {
		cam_rot.x -= rotate_speed;
	}
	if (Input::key(Key::A)) {
		cam_rot.y += rotate_speed;
	}
	if (Input::key(Key::D)) {
		cam_rot.y -= rotate_speed;
	}

	glm::mat4 view = glm::translate(glm::mat4(1.f), cam_pos) * glm::toMat4(glm::quat(cam_rot));
	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 800.f / 450.f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	// fill a GPU camera data struct
	static CameraUBO camData;
	camData.projection = projection;
	camData.view = view;
	camData.v_p = projection * view;

	// Camera Data
	MemoryMapper::effect_mmap(
		allocator, frame().camera_buffer, [](void* data) { memcpy(data, &camData, sizeof(CameraUBO)); });

	// Scene Data
	float framed = static_cast<float>(frame_number) / 120.f;
	scene_parameters.ambient_color = { sin(framed), 0, cos(framed), 1 };
	scene_parameters.fog = { sin(framed), 0, cos(framed), 1 };
	auto frame_index = frame_number % FRAME_OVERLAP;
	char* scene_data;
	vmaMapMemory(allocator, scene_parameter_buffer.allocation, (void**)&scene_data);
	scene_data += pad_uniform_buffer_size(sizeof(SceneUBO)) * frame_index;
	memcpy(scene_data, &scene_parameters, sizeof(SceneUBO));
	vmaUnmapMemory(allocator, scene_parameter_buffer.allocation);

	// SSBO data
	MemoryMapper::effect_mmap<ObjectData>(allocator, frame().object_buffer, [&](ObjectData* data) {
		for (auto& object : renderables) {
			data->model_matrix = object.transform;
			data++;
		}
	});

	std::shared_ptr<Mesh> last_bound_mesh = nullptr;
	std::shared_ptr<RenderMaterial> last_bound_material = nullptr;

	if (last_renderable_size != renderables.size()) {
		renderables_are_sorted = false;
	}

	if (!renderables_are_sorted) {
		std::sort(renderables.begin(), renderables.end(),
			[](const VulkanRenderObject& a, const VulkanRenderObject& b) -> int {
				if (a.material != b.material) {
					return true;
				}
				return false;
			});
		renderables_are_sorted = true;
	}

	for (int i = 0; i < renderables.size(); i++) {
		auto& object = renderables[i];
		// only bind the pipeline if it doesn't match with the already bound one
		if (object.material != last_bound_material) {
			last_bound_material = object.material;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, last_bound_material->pipeline);

			// Scene descriptor
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(SceneUBO)) * frame_index;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, last_bound_material->layout, 0, 1,
				&frame().global_descriptor, 1, &uniform_offset);

			// SSBO descriptor
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, last_bound_material->layout, 1, 1,
				&frame().object_descriptor, 0, nullptr);

			if (last_bound_material->texture_set) {
				// texture descriptor
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, last_bound_material->layout, 2, 1,
					&last_bound_material->texture_set, 0, nullptr);
			}
		}

		MeshPushConstants constants{};
		constants.render_matrix = object.transform;

		// upload the mesh to the GPU via push constants
		vkCmdPushConstants(
			cmd, object.material->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		// only bind the mesh if it's a different one from last bind
		VkDeviceSize offset = 0;
		if (object.mesh != last_bound_mesh) {
			// bind the mesh vertex buffer with offset 0
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->get_vertex_buffer().buffer, &offset);
			vkCmdBindIndexBuffer(cmd, object.mesh->get_index_buffer().buffer, 0, VK_INDEX_TYPE_UINT32);
			last_bound_mesh = object.mesh;
		}
		// we can now draw
		// vkCmdDraw(cmd, last_bound_mesh->get_vertices().size(), 1, 0, i);
		vkCmdDrawIndexed(cmd, last_bound_mesh->get_indices().size(), 1, offset, 0, i);
	}

	last_renderable_size = renderables.size();
}

void VulkanEngine::cleanup()
{
	// make sure the GPU has stopped doing its things
	VkFence all_fences[FRAME_OVERLAP];
	for (int i = 0; i < FRAME_OVERLAP; ++i) {
		all_fences[i] = frames_in_flight[i].render_fence;
	}
	vkWaitForFences(device(), FRAME_OVERLAP, all_fences, true, 1000000000);

	cleanup_queue.flush();

	vmaDestroyAllocator(allocator);

	vkDestroySurfaceKHR(instance(), VulkanContext::get_surface(), nullptr);

	vkDestroyDevice(device(), nullptr);
	vkDestroyInstance(instance(), nullptr);
}

} // ForgottenEngine
