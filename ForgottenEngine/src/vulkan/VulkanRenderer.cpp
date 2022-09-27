#include "fg_pch.hpp"

#include "vulkan/VulkanRenderer.hpp"

#include "Application.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"
#include "render/IndexBuffer.hpp"
#include "render/RenderCommandQueue.hpp"
#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "render/RendererCapabilites.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBuffer.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/compiler/VulkanShaderCompiler.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanFramebuffer.hpp"
#include "vulkan/VulkanIndexBuffer.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanRenderCommandBuffer.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

	namespace Utils {

		void insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange)
		{
			VkImageMemoryBarrier image_memory_barrier {};
			image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			image_memory_barrier.srcAccessMask = srcAccessMask;
			image_memory_barrier.dstAccessMask = dstAccessMask;
			image_memory_barrier.oldLayout = oldImageLayout;
			image_memory_barrier.newLayout = newImageLayout;
			image_memory_barrier.image = image;
			image_memory_barrier.subresourceRange = subresourceRange;

			vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
		}

		void set_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_image_layout, VkImageLayout new_image_layout,
			VkImageSubresourceRange subresource_range, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
		{
			// Create an image barrier object
			VkImageMemoryBarrier image_memory_barrier = {};
			image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.oldLayout = old_image_layout;
			image_memory_barrier.newLayout = new_image_layout;
			image_memory_barrier.image = image;
			image_memory_barrier.subresourceRange = subresource_range;

			// Source layouts (old)
			// Source access mask controls actions that have to be finished on the old layout
			// before it will be transitioned to the new layout
			switch (old_image_layout) {
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				image_memory_barrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
			switch (new_image_layout) {
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				image_memory_barrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (image_memory_barrier.srcAccessMask == 0) {
					image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Put barrier inside setup command buffer
			vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
		}

		void set_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags aspect_mask, VkImageLayout old_image_layout,
			VkImageLayout new_image_layout, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
		{
			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = aspect_mask;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = 1;
			subresource_range.layerCount = 1;
			set_image_layout(command_buffer, image, old_image_layout, new_image_layout, subresource_range, src_stage_mask, dst_stage_mask);
		}
	} // namespace Utils

	static const char* vulkan_vendor_to_identifier_string(uint32_t vendor_id)
	{
		switch (vendor_id) {
		case 0x10DE:
			return "NVIDIA";
		case 0x1002:
			return "AMD";
		case 0x8086:
			return "INTEL";
		case 0x13B5:
			return "ARM";
		}
		return "Unknown";
	}

	using PerFrameWriteDescriptor = std::vector<std::vector<VkWriteDescriptorSet>>;

	struct VulkanRendererData {
		RendererCapabilities render_caps;

		Reference<Texture2D> BRDFLut;

		Reference<VertexBuffer> QuadVertexBuffer;
		Reference<IndexBuffer> QuadIndexBuffer;
		VulkanShader::ShaderMaterialDescriptorSet QuadDescriptorSet;

		std::unordered_map<SceneRenderer*, std::vector<VulkanShader::ShaderMaterialDescriptorSet>> RendererDescriptorSet;
		VkDescriptorSet active_descriptor_set = nullptr;
		std::vector<VkDescriptorPool> descriptor_pools;
		std::vector<uint32_t> descriptor_pool_allocation_count;

		// UniformBufferSet -> Shader Hash -> Frame -> WriteDescriptor
		std::unordered_map<UniformBufferSet*, std::unordered_map<uint64_t, PerFrameWriteDescriptor>> uniform_buffer_write_descriptor_cache;
		std::unordered_map<StorageBufferSet*, std::unordered_map<uint64_t, PerFrameWriteDescriptor>> storage_buffer_write_descriptor_cache;

		// Default samplers
		VkSampler clamp_sampler = nullptr;
		VkSampler point_sampler = nullptr;

		int32_t selected_draw_call = -1;
		int32_t draw_call_count = 0;
	};

	static VulkanRendererData& renderer_data()
	{
		static VulkanRendererData* renderer_data_impl = nullptr;

		if (!renderer_data_impl) {
			renderer_data_impl = new VulkanRendererData;
		}

		return *renderer_data_impl;
	};

	static RenderCommandQueue& render_command_queue()
	{
		static RenderCommandQueue* render_command_queue_impl = nullptr;

		if (!render_command_queue_impl) {
			render_command_queue_impl = new RenderCommandQueue;
		}

		return *render_command_queue_impl;
	};

	static const std::vector<std::vector<VkWriteDescriptorSet>>& rt_retrieve_or_create_uniform_buffer_write_descriptors(
		Reference<UniformBufferSet> ubs, Reference<VulkanMaterial> vulkan_material)
	{
		size_t shader_hash = vulkan_material->get_shader()->get_hash();
		if (renderer_data().uniform_buffer_write_descriptor_cache.find(ubs.raw()) != renderer_data().uniform_buffer_write_descriptor_cache.end()) {
			const auto& shader_map = renderer_data().uniform_buffer_write_descriptor_cache.at(ubs.raw());
			if (shader_map.find(shader_hash) != shader_map.end()) {
				const auto& write_descriptors = shader_map.at(shader_hash);
				return write_descriptors;
			}
		}

		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
		Reference<VulkanShader> vulkan_shader = vulkan_material->get_shader().as<VulkanShader>();
		if (vulkan_shader->has_descriptor_set(0)) {
			const auto& shader_descriptor_sets = vulkan_shader->get_shader_descriptor_sets();
			if (!shader_descriptor_sets.empty()) {
				for (auto&& [binding, shaderUB] : shader_descriptor_sets[0].uniform_buffers) {
					auto& write_descriptors = renderer_data().uniform_buffer_write_descriptor_cache[ubs.raw()][shader_hash];
					write_descriptors.resize(frames_in_flight);
					for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
						Reference<VulkanUniformBuffer> uniform_buffer = ubs->get(binding, 0, frame); // set = 0 for now

						VkWriteDescriptorSet write_descriptor_set = {};
						write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						write_descriptor_set.descriptorCount = 1;
						write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						write_descriptor_set.pBufferInfo = &uniform_buffer->get_descriptor_buffer_info();
						write_descriptor_set.dstBinding = uniform_buffer->get_binding();
						write_descriptors[frame].push_back(write_descriptor_set);
					}
				}
			}
		}

		return renderer_data().uniform_buffer_write_descriptor_cache[ubs.raw()][shader_hash];
	}

	static const std::vector<std::vector<VkWriteDescriptorSet>>& rt_retrieve_or_create_storage_buffer_write_descriptors(
		Reference<StorageBufferSet> sbs, Reference<VulkanMaterial> vulkan_material)
	{
		size_t shader_hash = vulkan_material->get_shader()->get_hash();

		auto& map = renderer_data().storage_buffer_write_descriptor_cache;
		if (map.find(sbs.raw()) != map.end()) {
			const auto& shader_map = renderer_data().storage_buffer_write_descriptor_cache.at(sbs.raw());
			if (shader_map.find(shader_hash) != shader_map.end()) {
				const auto& write_descriptors = shader_map.at(shader_hash);
				return write_descriptors;
			}
		}

		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
		Reference<VulkanShader> vulkan_shader = vulkan_material->get_shader().as<VulkanShader>();
		if (vulkan_shader->has_descriptor_set(0)) {
			const auto& shader_descriptor_sets = vulkan_shader->get_shader_descriptor_sets();
			if (!shader_descriptor_sets.empty()) {
				for (auto&& [binding, shaderSB] : shader_descriptor_sets[0].storage_buffers) {
					auto& write_descriptors = renderer_data().storage_buffer_write_descriptor_cache[sbs.raw()][shader_hash];
					write_descriptors.resize(frames_in_flight);
					for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
						Reference<VulkanStorageBuffer> storage_buffer = sbs->get(binding, 0, frame); // set = 0 for now

						VkWriteDescriptorSet write_descriptor_set = {};
						write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						write_descriptor_set.descriptorCount = 1;
						write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						write_descriptor_set.pBufferInfo = &storage_buffer->get_descriptor_buffer_info();
						write_descriptor_set.dstBinding = storage_buffer->get_binding();
						write_descriptors[frame].push_back(write_descriptor_set);
					}
				}
			}
		}

		return renderer_data().storage_buffer_write_descriptor_cache[sbs.raw()][shader_hash];
	}

	void VulkanRenderer::init()
	{
		const auto& config = Renderer::get_config();
		renderer_data().descriptor_pools.resize(config.frames_in_flight);
		renderer_data().descriptor_pool_allocation_count.resize(config.frames_in_flight);

		auto& caps = renderer_data().render_caps;
		auto& properties = VulkanContext::get_current_device()->get_physical_device()->get_properties();
		caps.vendor = vulkan_vendor_to_identifier_string(properties.vendorID);
		caps.device = properties.deviceName;
		caps.version = std::to_string(properties.driverVersion);

		Renderer::submit([]() mutable {
			// Create Descriptor Pool
			VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 100 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 100000;
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
			uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
			for (uint32_t i = 0; i < frames_in_flight; i++) {
				vk_check(vkCreateDescriptorPool(device, &pool_info, nullptr, &renderer_data().descriptor_pools[i]));
				renderer_data().descriptor_pool_allocation_count[i] = 0;
			}
		});

		// Create fullscreen quad
		float x = -1;
		float y = -1;
		float width = 2, height = 2;
		struct QuadVertex {
			glm::vec3 Position;
			glm::vec2 TexCoord;
		};

		QuadVertex* data = new QuadVertex[4];

		data[0].Position = glm::vec3(x, y, 0.0f);
		data[0].TexCoord = glm::vec2(0, 0);

		data[1].Position = glm::vec3(x + width, y, 0.0f);
		data[1].TexCoord = glm::vec2(1, 0);

		data[2].Position = glm::vec3(x + width, y + height, 0.0f);
		data[2].TexCoord = glm::vec2(1, 1);

		data[3].Position = glm::vec3(x, y + height, 0.0f);
		data[3].TexCoord = glm::vec2(0, 1);

		auto data_vb = VertexBuffer::create(data, 4 * sizeof(QuadVertex));
		renderer_data().QuadVertexBuffer = data_vb;
		uint32_t indices[6] = {
			0,
			1,
			2,
			2,
			3,
			0,
		};
		renderer_data().QuadIndexBuffer = IndexBuffer::create(indices, 6 * sizeof(uint32_t));
	};

	void VulkanRenderer::shut_down()
	{
		auto device = VulkanContext::get_current_device()->get_vulkan_device();
		vkDeviceWaitIdle(device);

		VulkanShaderCompiler::clear_uniform_buffers();
	};

	void VulkanRenderer::begin_frame()
	{
		Renderer::submit([]() {
			auto& swap_chain = Application::the().get_window().get_swapchain();

			// Reset descriptor pools here
			VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
			uint32_t buffer_index = swap_chain.get_current_buffer_index();
			CORE_INFO("{}", buffer_index);
			vkResetDescriptorPool(device, renderer_data().descriptor_pools[buffer_index], 0);
			memset(renderer_data().descriptor_pool_allocation_count.data(), 0,
				renderer_data().descriptor_pool_allocation_count.size() * sizeof(uint32_t));

			renderer_data().draw_call_count = 0;
		});
	};

	void VulkanRenderer::end_frame() { }

	void VulkanRenderer::begin_render_pass(Reference<RenderCommandBuffer> in_command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
	{
		Renderer::submit([in_command_buffer, render_pass, explicit_clear]() {
			VkCommandBuffer command_buffer = in_command_buffer.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

			auto& fb = render_pass->get_specification().target_framebuffer;
			Reference<VulkanFramebuffer> framebuffer = fb.as<VulkanFramebuffer>();
			const auto& fb_spec = framebuffer->get_specification();

			uint32_t width = framebuffer->get_width();
			uint32_t height = framebuffer->get_height();

			VkViewport viewport = {};
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRenderPassBeginInfo render_pass_begin_info = {};
			render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_begin_info.pNext = nullptr;
			render_pass_begin_info.renderPass = framebuffer->get_render_pass();
			render_pass_begin_info.renderArea.offset.x = 0;
			render_pass_begin_info.renderArea.offset.y = 0;
			render_pass_begin_info.renderArea.extent.width = width;
			render_pass_begin_info.renderArea.extent.height = height;

			if (framebuffer->get_specification().swapchain_target) {
				VulkanSwapchain& swap_chain = Application::the().get_window().get_swapchain();
				width = swap_chain.get_width();
				height = swap_chain.get_height();
				render_pass_begin_info.framebuffer = swap_chain.get_current_framebuffer();

				viewport.x = 0.0f;
				viewport.y = (float)height;
				viewport.width = (float)width;
				viewport.height = (float)height;
			} else {
				width = framebuffer->get_width();
				height = framebuffer->get_height();
				render_pass_begin_info.framebuffer = framebuffer->get_vulkan_framebuffer();

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)width;
				viewport.height = (float)height;
			}

			// TODO: Does our framebuffer have a depth attachment?
			const auto& clear_values = framebuffer->get_vulkan_clear_values();
			render_pass_begin_info.clearValueCount = (uint32_t)clear_values.size();
			render_pass_begin_info.pClearValues = clear_values.data();

			vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

			if (explicit_clear) {
				const auto colour_attachment_count = (uint32_t)framebuffer->get_color_attachment_count();
				const uint32_t total_attachment_count = colour_attachment_count + (framebuffer->has_depth_attachment() ? 1 : 0);
				core_assert_bool(clear_values.size() == total_attachment_count);

				std::vector<VkClearAttachment> attachments(total_attachment_count);
				std::vector<VkClearRect> clear_rects(total_attachment_count);
				for (uint32_t i = 0; i < colour_attachment_count; i++) {
					attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					attachments[i].colorAttachment = i;
					attachments[i].clearValue = clear_values[i];

					clear_rects[i].rect.offset = { (int32_t)0, (int32_t)0 };
					clear_rects[i].rect.extent = { width, height };
					clear_rects[i].baseArrayLayer = 0;
					clear_rects[i].layerCount = 1;
				}

				if (framebuffer->has_depth_attachment()) {
					attachments[colour_attachment_count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
					attachments[colour_attachment_count].clearValue = clear_values[colour_attachment_count];
					clear_rects[colour_attachment_count].rect.offset = { (int32_t)0, (int32_t)0 };
					clear_rects[colour_attachment_count].rect.extent = { width, height };
					clear_rects[colour_attachment_count].baseArrayLayer = 0;
					clear_rects[colour_attachment_count].layerCount = 1;
				}

				vkCmdClearAttachments(command_buffer, total_attachment_count, attachments.data(), total_attachment_count, clear_rects.data());
			}

			// Update dynamic viewport state
			vkCmdSetViewport(command_buffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(command_buffer, 0, 1, &scissor);
		});
	}

	void VulkanRenderer::render_geometry(Reference<RenderCommandBuffer> command_buffer, Reference<Pipeline> pipeline, Reference<UniformBufferSet> ubs,
		Reference<StorageBufferSet> sbs, Reference<Material> material, Reference<VertexBuffer> vb, Reference<IndexBuffer> ib,
		const glm::mat4& transform, uint32_t index_count)
	{
		Reference<VulkanMaterial> vulkan_material = material.as<VulkanMaterial>();
		if (index_count == 0)
			index_count = ib->get_count();

		Renderer::submit([command_buffer, pipeline, ubs, vulkan_material, vb, ib, transform, index_count]() mutable {
			VkCommandBuffer render_command_buffer = command_buffer.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

			Reference<VulkanPipeline> vulkan_pipeline = pipeline.as<VulkanPipeline>();

			VkPipelineLayout layout = vulkan_pipeline->get_vulkan_pipeline_layout();

			VkPipeline vk_pipeline = vulkan_pipeline->get_vulkan_pipeline();
			vkCmdBindPipeline(render_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

			const auto& write_descriptors = rt_retrieve_or_create_uniform_buffer_write_descriptors(ubs, vulkan_material);
			vulkan_material->rt_update_for_rendering(write_descriptors);
			auto vulkan_mesh_vb = vb.as<VulkanVertexBuffer>();
			VkBuffer vb_mesh_buffer = vulkan_mesh_vb->get_vulkan_buffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(render_command_buffer, 0, 1, &vb_mesh_buffer, offsets);

			auto vulkan_mesh_ib = ib.as<VulkanIndexBuffer>();
			VkBuffer ib_mesh_buffer = vulkan_mesh_ib->get_vulkan_buffer();
			vkCmdBindIndexBuffer(render_command_buffer, ib_mesh_buffer, 0, VK_INDEX_TYPE_UINT32);

			uint32_t buffer_index = Renderer::get_current_frame_index();
			VkDescriptorSet descriptor_set = vulkan_material->get_descriptor_set(buffer_index);
			if (descriptor_set)
				vkCmdBindDescriptorSets(render_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor_set, 0, nullptr);

			vkCmdPushConstants(render_command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
			Buffer uniform_storage_buffers = vulkan_material->get_uniform_storage_buffer();
			if (uniform_storage_buffers)
				vkCmdPushConstants(render_command_buffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), uniform_storage_buffers.size,
					uniform_storage_buffers.data);

			vkCmdDrawIndexed(render_command_buffer, index_count, 1, 0, 0, 0);
		});
	}

	void VulkanRenderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer, const Reference<Pipeline>& pipeline_in,
		const Reference<UniformBufferSet>& ub, const Reference<StorageBufferSet>& sb, const Reference<Material>& material)
	{

		Reference<VulkanMaterial> vulkan_material = material.as<VulkanMaterial>();
		Renderer::submit([this, command_buffer, pipe = pipeline_in, ubs = ub, sbs = sb, vulkan_material]() mutable {
			VkCommandBuffer commandBuffer = command_buffer.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

			Reference<VulkanPipeline> vulkanPipeline = pipe.as<VulkanPipeline>();

			VkPipelineLayout layout = vulkanPipeline->get_vulkan_pipeline_layout();

			auto vulkanMeshVB = renderer_data().QuadVertexBuffer.as<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->get_vulkan_buffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = renderer_data().QuadIndexBuffer.as<VulkanIndexBuffer>();
			VkBuffer ibBuffer = vulkanMeshIB->get_vulkan_buffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			VkPipeline pipeline = vulkanPipeline->get_vulkan_pipeline();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			this->rt_update_material_for_rendering(vulkan_material, ubs, sbs);

			uint32_t bufferIndex = Renderer::get_current_frame_index();
			VkDescriptorSet descriptorSet = vulkan_material->get_descriptor_set(bufferIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

			Buffer uniformStorageBuffer = vulkan_material->get_uniform_storage_buffer();
			if (uniformStorageBuffer.size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, uniformStorageBuffer.size, uniformStorageBuffer.data);

			vkCmdDrawIndexed(commandBuffer, renderer_data().QuadIndexBuffer->get_count(), 1, 0, 0, 0);
		});
	}

	void VulkanRenderer::end_render_pass(Reference<RenderCommandBuffer> command_buffer)
	{
		Renderer::submit([cmd_buffer = command_buffer]() {
			VkCommandBuffer command_buffer = cmd_buffer.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

			vkCmdEndRenderPass(command_buffer);
		});
	}

	void VulkanRenderer::rt_update_material_for_rendering(
		Reference<VulkanMaterial> material, Reference<UniformBufferSet> ubs, Reference<StorageBufferSet> sbs)
	{
		if (ubs) {
			auto write_descriptors = rt_retrieve_or_create_uniform_buffer_write_descriptors(ubs, material);
			if (sbs) {
				const auto& storage_buffer_write_descriptors = rt_retrieve_or_create_storage_buffer_write_descriptors(sbs, material);

				const uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
				for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
					write_descriptors[frame].reserve(write_descriptors[frame].size() + storage_buffer_write_descriptors[frame].size());
					write_descriptors[frame].insert(write_descriptors[frame].end(), storage_buffer_write_descriptors[frame].begin(),
						storage_buffer_write_descriptors[frame].end());
				}
			}
			material->rt_update_for_rendering(write_descriptors);
		} else {
			material->rt_update_for_rendering();
		}
	}

	VkDescriptorSet VulkanRenderer::rt_allocate_descriptor_set(VkDescriptorSetAllocateInfo alloc_info)
	{
		uint32_t buffer_index = Renderer::get_current_frame_index();
		alloc_info.descriptorPool = renderer_data().descriptor_pools[buffer_index];
		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		VkDescriptorSet result;
		vk_check(vkAllocateDescriptorSets(device, &alloc_info, &result));
		renderer_data().descriptor_pool_allocation_count[buffer_index] += alloc_info.descriptorSetCount;
		return result;
	}

	void VulkanRenderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer, const Reference<Pipeline>& pipeline,
		const Reference<UniformBufferSet>& ubs, const Reference<Material>& material)
	{
		submit_fullscreen_quad(command_buffer, pipeline, ubs, nullptr, material);
	}

}; // namespace ForgottenEngine
