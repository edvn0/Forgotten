//
// Created by Edwin Carlsson on 2022-07-02.
//

#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

class VulkanPipelineBuilder;

struct VulkanPipeline {
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
	VkPipelineVertexInputStateCreateInfo vertex_input_info;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipeline_layout;

	friend class VulkanPipelineBuilder;
	static VulkanPipelineBuilder create();
};

class VulkanPipelineBuilder {
private:
	VulkanPipeline pipeline;

public:
	VkPipeline build(VkRenderPass pass);

	void clear_shader_stages() { pipeline.shader_stages.clear(); }

	VulkanPipelineBuilder& with_stage(VkPipelineShaderStageCreateInfo info)
	{
		pipeline.shader_stages.push_back(info);
		return *this;
	}

	VulkanPipelineBuilder& with_vertex_input(VkPipelineVertexInputStateCreateInfo info)
	{
		pipeline.vertex_input_info = info;
		return *this;
	}

	VulkanPipelineBuilder& with_input_assembly(VkPipelineInputAssemblyStateCreateInfo info)
	{
		pipeline.input_assembly = info;
		return *this;
	}

	VulkanPipelineBuilder& with_viewport(VkViewport info)
	{
		pipeline.viewport = info;
		return *this;
	}

	VulkanPipelineBuilder& with_scissors(VkRect2D info)
	{
		pipeline.scissor = info;
		return *this;
	}

	VulkanPipelineBuilder& with_rasterizer(VkPipelineRasterizationStateCreateInfo info)
	{
		pipeline.rasterizer = info;
		return *this;
	}

	VulkanPipelineBuilder& with_color_blend(VkPipelineColorBlendAttachmentState info)
	{
		pipeline.color_blend_attachment = info;
		return *this;
	}

	VulkanPipelineBuilder& with_multisampling(VkPipelineMultisampleStateCreateInfo info)
	{
		pipeline.multisampling = info;
		return *this;
	}

	VulkanPipelineBuilder& with_pipeline_layout(VkPipelineLayout info)
	{
		pipeline.pipeline_layout = info;
		return *this;
	}
};

} // ForgottenEngine