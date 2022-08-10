#pragma once

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanIndexBuffer.hpp"
#include "vulkan/VulkanMaterial.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/VulkanStorageBuffer.hpp"
#include "vulkan/VulkanUniformBuffer.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"
#include "vulkan/vulkan.h"

namespace ForgottenEngine {

	class SceneRenderer;

	namespace Utils {
		void insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcaccessmask,
			VkAccessFlags dstaccessmask, VkImageLayout oldimagelayout, VkImageLayout newimagelayout,
			VkPipelineStageFlags srcstagemask, VkPipelineStageFlags dststagemask,
			VkImageSubresourceRange subresourcerange);

		void set_image_layout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldimagelayout,
			VkImageLayout newimagelayout, VkImageSubresourceRange subresourcerange,
			VkPipelineStageFlags srcstagemask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dststagemask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		void set_image_layout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectmask,
			VkImageLayout oldimagelayout, VkImageLayout newimagelayout,
			VkPipelineStageFlags srcstagemask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dststagemask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	} // namespace Utils

	class VulkanRenderer : public RendererAPI {
	public:
		~VulkanRenderer() override = default;

		void init() override;
		void shut_down() override;

	public:
		void begin_frame() override;

		void begin_render_pass(Reference<RenderCommandBuffer> command_buffer, Reference<RenderPass> render_pass,
			bool explicit_clear) override;

		void end_render_pass(Reference<RenderCommandBuffer> command_buffer) override;

		void end_frame() override;

	public:
		// SUBMITS
		void render_geometry(Reference<RenderCommandBuffer> command_buffer, Reference<Pipeline> pipeline,
			Reference<UniformBufferSet> ubs, Reference<StorageBufferSet> sbs, Reference<Material> material,
			Reference<VertexBuffer> vb, Reference<IndexBuffer> ib, const glm::mat4& transform,
			uint32_t index_count) override;

		void submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
			const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& uniformBufferSet,
			const Reference<Material>& material) override;
		void submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
			const Reference<Pipeline>& pipeline_in, const Reference<UniformBufferSet>& ub,
			const Reference<StorageBufferSet>& sb, const Reference<Material>& material) override;
		// END SUBMITS
	public:
		static VkDescriptorSet rt_allocate_descriptor_set(VkDescriptorSetAllocateInfo info);

	public:
		void rt_update_material_for_rendering(Reference<VulkanMaterial> material,
			Reference<UniformBufferSet> uniformBufferSet, Reference<StorageBufferSet> storageBufferSet);
	};

} // namespace ForgottenEngine
