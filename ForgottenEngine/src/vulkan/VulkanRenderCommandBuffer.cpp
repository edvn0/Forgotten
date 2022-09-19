//
// Created by Edwin Carlsson on 2022-07-26.
//

#include "vulkan/VulkanRenderCommandBuffer.hpp"

#include "Application.hpp"
#include "ApplicationProperties.hpp"
#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	template <typename OnTrue, typename OnFalse> static constexpr auto do_if(bool expr, OnTrue&& on_true, OnFalse&& on_false)
	{
		if (expr) {
			on_true();
		} else {
			on_false();
		}
	}

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count)
	{
		auto device = VulkanContext::get_current_device();
		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;

		VkCommandPoolCreateInfo cpci = {};
		cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpci.queueFamilyIndex = device->get_physical_device()->get_queue_family_indices().graphics;
		cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(device->get_vulkan_device(), &cpci, nullptr, &command_pool));

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
		Reference<VulkanRenderCommandBuffer> instance = this;
		Renderer::submit([instance]() mutable {
			uint32_t frame_index = Renderer::get_current_frame_index();

			VkCommandBufferBeginInfo cbi = {};
			cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			cbi.pNext = nullptr;

			VkCommandBuffer cmd_buffer;
			do_if(
				instance->owned_by_swapchain,
				[&cmd_buffer, frame_index] { cmd_buffer = Application::the().get_window().get_swapchain().get_drawbuffer(frame_index); },
				[&cmd_buffer, frame_index, &instance] { cmd_buffer = instance->command_buffers[frame_index]; });
			instance->active_command_buffer = cmd_buffer;
			VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &cbi));
		});
	}

	void VulkanRenderCommandBuffer::end()
	{
		Reference<VulkanRenderCommandBuffer> instance = this;
		Renderer::submit([instance]() mutable {
			VkCommandBuffer cmd_buffer = instance->active_command_buffer;
			VK_CHECK(vkEndCommandBuffer(cmd_buffer));

			instance->active_command_buffer = nullptr;
		});
	}

	void VulkanRenderCommandBuffer::submit()
	{
		if (owned_by_swapchain)
			return;
		Reference<VulkanRenderCommandBuffer> instance = this;
		Renderer::submit([instance]() mutable {
			auto device = VulkanContext::get_current_device();

			uint32_t frame_index = Renderer::get_current_frame_index();

			VkSubmitInfo submit_info {};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.commandBufferCount = 1;
			VkCommandBuffer cmd_buffer = instance->command_buffers[frame_index];
			submit_info.pCommandBuffers = &cmd_buffer;

			VK_CHECK(vkWaitForFences(device->get_vulkan_device(), 1, &instance->wait_fences[frame_index], VK_TRUE, UINT64_MAX));
			VK_CHECK(vkResetFences(device->get_vulkan_device(), 1, &instance->wait_fences[frame_index]));
			VK_CHECK(vkQueueSubmit(device->get_graphics_queue(), 1, &submit_info, instance->wait_fences[frame_index]));
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