//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanPipelineBuilder.hpp"

#include "Logger.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

VulkanPipelineBuilder VulkanPipeline::create() { return {}; }

VkPipeline VulkanPipelineBuilder::build(VkRenderPass pass)
{
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &pipeline.viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &pipeline.scissor;

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.pNext = nullptr;

	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &pipeline.color_blend_attachment;

	VkGraphicsPipelineCreateInfo pi = {};
	pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pi.pNext = nullptr;

	pi.stageCount = pipeline.shader_stages.size();
	pi.pStages = pipeline.shader_stages.data();
	pi.pVertexInputState = &pipeline.vertex_input_info;
	pi.pInputAssemblyState = &pipeline.input_assembly;
	pi.pViewportState = &viewportState;
	pi.pRasterizationState = &pipeline.rasterizer;
	pi.pMultisampleState = &pipeline.multisampling;
	pi.pColorBlendState = &color_blending;
	pi.pDepthStencilState = &pipeline.depth_stencil;
	pi.layout = pipeline.pipeline_layout;
	pi.renderPass = pass;
	pi.subpass = 0;
	pi.basePipelineHandle = nullptr;

	VkPipeline created_pipeline;
	auto res = vkCreateGraphicsPipelines(VulkanContext::get_device(), nullptr, 1, &pi, nullptr, &created_pipeline);
	if (res != VK_SUCCESS) {
		CORE_ERROR("Failed to create pipeline. Potential reason: {}", res);
		return nullptr; // failed to create graphics pipeline
	} else {
		return created_pipeline;
	}
}

} // ForgottenEngine