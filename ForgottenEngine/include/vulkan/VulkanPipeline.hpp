#pragma once

#include <utility>

#include "render/Pipeline.hpp"

namespace ForgottenEngine {

class VulkanPipeline : public Pipeline {
public:
	explicit VulkanPipeline(PipelineSpecification spec)
		: spec(std::move(spec)){};

	~VulkanPipeline() override = default;

	void bind() override;
	void invalidate() override;

	PipelineSpecification& get_specification() override { return spec; }
	const PipelineSpecification& get_specification() const override { return spec; }

	VkPipelineLayout get_vulkan_pipeline_layout() const { return layout; }
	VkPipeline get_vulkan_pipeline() const { return pipeline; }

protected:
	void set_uniform_buffer_impl(Reference<UniformBuffer> uniformBuffer, uint32_t binding, uint32_t set) override;

private:
	PipelineSpecification spec;
	VkPipelineLayout layout{};
	VkPipeline pipeline{};
};

}