#pragma once

#include "VulkanShader.hpp"
#include "render/Pipeline.hpp"

#include <utility>

namespace ForgottenEngine {

	class VulkanPipeline : public Pipeline {
	public:
		explicit VulkanPipeline(const PipelineSpecification& spec);

		~VulkanPipeline() override;

		void bind() override;
		void invalidate() override;

		PipelineSpecification& get_specification() override { return spec; }
		const PipelineSpecification& get_specification() const override { return spec; }

		VkPipelineLayout get_vulkan_pipeline_layout() const { return pipeline_layout; }
		VkPipeline get_vulkan_pipeline() const { return pipeline; }

		auto get_descriptor_set(uint32_t set) { return descriptor_sets.descriptor_sets[set]; }

		void set_uniform_buffer(Reference<UniformBuffer> ub, uint32_t binding, uint32_t set) override;
		void rt_set_uniform_buffer(Reference<UniformBuffer> ub, uint32_t binding, uint32_t set = 0);

	private:
		PipelineSpecification spec;
		VkPipelineLayout pipeline_layout {};
		VkPipeline pipeline {};
		VkPipelineCache pipeline_cache = nullptr;
		VulkanShader::ShaderMaterialDescriptorSet descriptor_sets;
	};

} // namespace ForgottenEngine