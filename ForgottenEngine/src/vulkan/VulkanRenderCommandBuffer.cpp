//
// Created by Edwin Carlsson on 2022-07-26.
//

#include "vulkan/VulkanRenderCommandBuffer.hpp"

#include "Application.hpp"
#include "ApplicationProperties.hpp"
#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count)
	{
		auto device = VulkanContext::get_current_device();
		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;

		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->get_physical_device()->get_queue_family_indices().graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(device->get_vulkan_device(), &cmdPoolInfo, nullptr, &command_pool));

		VkCommandBufferAllocateInfo cbai {};
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = command_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		if (count == 0)
			count = frames_in_flight;
		cbai.commandBufferCount = count;
		command_buffers.resize(count);
		VK_CHECK(vkAllocateCommandBuffers(device->get_vulkan_device(), &cbai, command_buffers.data()));

		VkFenceCreateInfo fci {};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		wait_fences.resize(frames_in_flight);
		for (auto& wait_fence : wait_fences) {
			VK_CHECK(vkCreateFence(device->get_vulkan_device(), &fci, nullptr, &wait_fence));
		}

		create_performance_queries();
	}

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer()
		: owned_by_swapchain(true)
	{
		create_performance_queries();
	};

	void VulkanRenderCommandBuffer::create_performance_queries()
	{
#ifdef FORGOTTEN_WINDOWS
		auto frames_in_flight = Renderer::get_config().frames_in_flight;
		auto device = VulkanContext::get_current_device()->get_vulkan_device();

		VkQueryPoolCreateInfo queryPoolCreateInfo = {};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.pNext = nullptr;

		// Timestamp queries
		const uint32_t maxUserQueries = 16;
		timestamp_query_count = 2 + 2 * maxUserQueries;

		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolCreateInfo.queryCount = timestamp_query_count;
		timestamp_query_pools.resize(frames_in_flight);
		for (auto& tqp : timestamp_query_pools)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &tqp));

		timestamp_query_results.resize(frames_in_flight);
		for (auto& timestampQueryResults : timestamp_query_results)
			timestampQueryResults.resize(timestamp_query_count);

		gpu_execution_times.resize(frames_in_flight);
		for (auto& executionGPUTimes : gpu_execution_times)
			executionGPUTimes.resize(timestamp_query_count / 2);

		// Pipeline statistics queries
		pipeline_query_count = 7;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		queryPoolCreateInfo.queryCount = pipeline_query_count;
		queryPoolCreateInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
			| VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
			| VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
			| VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

		pipeline_statistics_query_pools.resize(frames_in_flight);
		for (auto& pipelineStatisticsQueryPools : pipeline_statistics_query_pools)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &pipelineStatisticsQueryPools));

		pipeline_statistics_query_results.resize(frames_in_flight);
#endif
	}

	void VulkanRenderCommandBuffer::begin()
	{
		Renderer::submit([this]() mutable {
			uint32_t frameIndex = Renderer::get_current_frame_index();

			VkCommandBufferBeginInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			cmdBufInfo.pNext = nullptr;

			VkCommandBuffer commandBuffer = nullptr;
			if (this->owned_by_swapchain) {
				auto& swapChain = Application::the().get_window().get_swapchain();
				commandBuffer = swapChain.get_drawbuffer(frameIndex);
			} else {
				commandBuffer = this->command_buffers[frameIndex];
			}
			this->active_command_buffer = commandBuffer;
			VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));
		});
	}

	void VulkanRenderCommandBuffer::end()
	{
		Renderer::submit([this]() mutable {
			uint32_t frameIndex = Renderer::get_current_frame_index();
			VkCommandBuffer commandBuffer = this->active_command_buffer;
			VK_CHECK(vkEndCommandBuffer(commandBuffer));

			this->active_command_buffer = nullptr;
		});
	}

	void VulkanRenderCommandBuffer::submit()
	{
		if (owned_by_swapchain)
			return;

		Renderer::submit([this]() mutable {
			auto device = VulkanContext::get_current_device();

			uint32_t frameIndex = Renderer::get_current_frame_index();

			VkSubmitInfo submitInfo {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			VkCommandBuffer commandBuffer = this->command_buffers[frameIndex];
			submitInfo.pCommandBuffers = &commandBuffer;

			VK_CHECK(vkWaitForFences(device->get_vulkan_device(), 1, &this->wait_fences[frameIndex], VK_TRUE, UINT64_MAX));
			VK_CHECK(vkResetFences(device->get_vulkan_device(), 1, &this->wait_fences[frameIndex]));
			VK_CHECK(vkQueueSubmit(device->get_graphics_queue(), 1, &submitInfo, this->wait_fences[frameIndex]));
		});
	}

	VulkanRenderCommandBuffer::~VulkanRenderCommandBuffer()
	{
		if (owned_by_swapchain)
			return;

		VkCommandPool commandPool = command_pool;
		Renderer::submit_resource_free([commandPool]() {
			auto device = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyCommandPool(device, commandPool, nullptr);
		});
	}

} // namespace ForgottenEngine