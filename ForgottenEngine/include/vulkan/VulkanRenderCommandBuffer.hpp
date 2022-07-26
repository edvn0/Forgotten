#pragma once

#include "render/RenderCommandBuffer.hpp"

#include <vector>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

class VulkanRenderCommandBuffer : public RenderCommandBuffer {
public:
	explicit VulkanRenderCommandBuffer(uint32_t count);
	VulkanRenderCommandBuffer();
	~VulkanRenderCommandBuffer() override = default;

	void begin() override;
	void end() override;
	void submit() override;

private:
	VkCommandPool command_pool = nullptr;
	std::vector<VkCommandBuffer> command_buffers;
	VkCommandBuffer active_command_buffer = nullptr;
	std::vector<VkFence> wait_fences;

	bool owned_by_swapchain = false;
};

}