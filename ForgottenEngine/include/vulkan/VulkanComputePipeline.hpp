//
// Created by Edwin Carlsson on 2022-08-10.
//

#pragma once

#pragma once

#include "render/ComputePipeline.hpp"
#include "vulkan/VulkanRenderCommandBuffer.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/VulkanTexture.hpp"
#include "vulkan/vulkan.h"

namespace ForgottenEngine {

	class VulkanComputePipeline : public ComputePipeline {
	public:
		explicit VulkanComputePipeline(const Reference<Shader>& computeShader);
		~VulkanComputePipeline() = default;

		void execute(VkDescriptorSet* descriptorSets, uint32_t descriptorSetCount, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void begin(Reference<RenderCommandBuffer> renderCommandBuffer) override;
		void dispatch(VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;
		void end() override;

		Reference<Shader> get_shader() override { return shader; }

		VkCommandBuffer get_active_command_buffer() { return active_command_buffer; }

		void set_push_constants(const void* data, uint32_t size) const;
		void create_pipeline();

	private:
		void rt_create_pipeline();

	private:
		Reference<VulkanShader> shader;

		VkPipelineLayout compute_layout = nullptr;
		VkPipelineCache compute_cache = nullptr;
		VkPipeline compute_pipeline = nullptr;

		VkCommandBuffer active_command_buffer = nullptr;

		bool using_graphics_queue = false;
	};

} // namespace ForgottenEngine