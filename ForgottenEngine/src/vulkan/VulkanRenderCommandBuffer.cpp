//
// Created by Edwin Carlsson on 2022-07-26.
//

#include "vulkan/VulkanRenderCommandBuffer.hpp"

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

	VkCommandBufferAllocateInfo cbai{};
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = command_pool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	if (count == 0)
		count = frames_in_flight;
	cbai.commandBufferCount = count;
	command_buffers.resize(count);
	VK_CHECK(vkAllocateCommandBuffers(device->get_vulkan_device(), &cbai, command_buffers.data()));

	VkFenceCreateInfo fci{};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	wait_fences.resize(frames_in_flight);
	for (auto& wait_fence : wait_fences) {
		VK_CHECK(vkCreateFence(device->get_vulkan_device(), &fci, nullptr, &wait_fence));
	}

	VkQueryPoolCreateInfo queryPoolCreateInfo = {};
	queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolCreateInfo.pNext = nullptr;
}

VulkanRenderCommandBuffer::VulkanRenderCommandBuffer() : owned_by_swapchain(true) {};

void VulkanRenderCommandBuffer::begin() { }

void VulkanRenderCommandBuffer::end() { }

void VulkanRenderCommandBuffer::submit() { }

}