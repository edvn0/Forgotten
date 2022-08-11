//
// Created by Edwin Carlsson on 2022-08-10.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanComputePipeline.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	static VkFence compute_fence = nullptr;

	VulkanComputePipeline::VulkanComputePipeline(Reference<Shader> computeShader)
		: shader(computeShader.as<VulkanShader>())
	{
		Reference<VulkanComputePipeline> instance = this;
		Renderer::submit([instance]() mutable {
			instance->rt_create_pipeline();
		});
		Renderer::register_shader_dependency(computeShader, this);
	}

	void VulkanComputePipeline::create_pipeline()
	{
		Renderer::submit([instance = Reference(this)]() mutable {
			instance->rt_create_pipeline();
		});
	}

	void VulkanComputePipeline::rt_create_pipeline()
	{
		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		// TODO: Abstract into some sort of compute pipeline

		auto descriptorSetLayouts = shader->get_all_descriptor_set_layouts();

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		const auto& pushConstantRanges = shader->get_push_constant_ranges();
		std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());
		if (!pushConstantRanges.empty()) {
			// TODO: should come from shader
			for (uint32_t i = 0; i < pushConstantRanges.size(); i++) {
				const auto& pushConstantRange = pushConstantRanges[i];
				auto& vulkanPushConstantRange = vulkanPushConstantRanges[i];

				vulkanPushConstantRange.stageFlags = pushConstantRange.ShaderStage;
				vulkanPushConstantRange.offset = pushConstantRange.Offset;
				vulkanPushConstantRange.size = pushConstantRange.Size;
			}

			pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)vulkanPushConstantRanges.size();
			pipelineLayoutCreateInfo.pPushConstantRanges = vulkanPushConstantRanges.data();
		}

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute_layout));

		VkComputePipelineCreateInfo computePipelineCreateInfo {};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = compute_layout;
		computePipelineCreateInfo.flags = 0;
		const auto& shaderStages = shader->get_pipeline_shader_stage_create_infos();
		computePipelineCreateInfo.stage = shaderStages[0];

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &compute_cache));
		VK_CHECK(vkCreateComputePipelines(device, compute_cache, 1, &computePipelineCreateInfo, nullptr, &compute_pipeline));
	}

	void VulkanComputePipeline::execute(VkDescriptorSet* descriptorSets, uint32_t descriptorSetCount, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkQueue computeQueue = VulkanContext::get_current_device()->get_compute_queue();
		// vkQueueWaitIdle(computeQueue); // TODO: don't

		VkCommandBuffer computeCommandBuffer = VulkanContext::get_current_device()->get_command_buffer(true, true);

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
		for (uint32_t i = 0; i < descriptorSetCount; i++) {
			vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_layout, 0, 1, &descriptorSets[i], 0, 0);
			vkCmdDispatch(computeCommandBuffer, groupCountX, groupCountY, groupCountZ);
		}

		vkEndCommandBuffer(computeCommandBuffer);
		if (!compute_fence) {

			VkFenceCreateInfo fenceCreateInfo {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &compute_fence));
		}

		// Make sure previous compute shader in pipeline has completed (TODO: this shouldn't be needed for all cases)
		vkWaitForFences(device, 1, &compute_fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &compute_fence);

		VkSubmitInfo computeSubmitInfo {};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
		VK_CHECK(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, compute_fence));

		// Wait for execution of compute shader to complete
		// Currently this is here for "safety"
		{
			vkWaitForFences(device, 1, &compute_fence, VK_TRUE, UINT64_MAX);
		}
	}

	void VulkanComputePipeline::begin(Reference<RenderCommandBuffer> renderCommandBuffer)
	{
		CORE_ASSERT(!active_command_buffer, "");

		if (renderCommandBuffer) {
			uint32_t frameIndex = Renderer::get_current_frame_index();
			active_command_buffer = renderCommandBuffer.as<VulkanRenderCommandBuffer>()->get_command_buffer(frameIndex);
			using_graphics_queue = true;
		} else {
			active_command_buffer = VulkanContext::get_current_device()->get_command_buffer(true, true);
			using_graphics_queue = false;
		}
		vkCmdBindPipeline(active_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
	}

	void VulkanComputePipeline::dispatch(VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
	{
		CORE_ASSERT(active_command_buffer, "");

		vkCmdBindDescriptorSets(active_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_layout, 0, 1, &descriptorSet, 0, 0);
		vkCmdDispatch(active_command_buffer, groupCountX, groupCountY, groupCountZ);
	}

	void VulkanComputePipeline::end()
	{
		CORE_ASSERT(active_command_buffer, "");

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		if (!using_graphics_queue) {
			VkQueue computeQueue = VulkanContext::get_current_device()->get_compute_queue();

			vkEndCommandBuffer(active_command_buffer);

			if (!compute_fence) {
				VkFenceCreateInfo fenceCreateInfo {};
				fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &compute_fence));
			}
			vkWaitForFences(device, 1, &compute_fence, VK_TRUE, UINT64_MAX);
			vkResetFences(device, 1, &compute_fence);

			VkSubmitInfo computeSubmitInfo {};
			computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo.commandBufferCount = 1;
			computeSubmitInfo.pCommandBuffers = &active_command_buffer;
			VK_CHECK(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, compute_fence));

			// Wait for execution of compute shader to complete
			// Currently this is here for "safety"
			{
				vkWaitForFences(device, 1, &compute_fence, VK_TRUE, UINT64_MAX);
			}
		}
		active_command_buffer = nullptr;
	}

	void VulkanComputePipeline::set_push_constants(const void* data, uint32_t size) const
	{
		vkCmdPushConstants(active_command_buffer, compute_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, data);
	}

} // namespace ForgottenEngine
