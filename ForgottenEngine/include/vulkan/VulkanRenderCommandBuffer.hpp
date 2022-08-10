#pragma once

#include "render/RenderCommandBuffer.hpp"

#include <vector>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

	class VulkanRenderCommandBuffer : public RenderCommandBuffer {
	public:
		explicit VulkanRenderCommandBuffer(uint32_t count);
		VulkanRenderCommandBuffer();
		~VulkanRenderCommandBuffer() override;

		void begin() override;
		void end() override;
		void submit() override;

		VkCommandBuffer get_active_command_buffer() const { return active_command_buffer; }

		VkCommandBuffer get_command_buffer(uint32_t frame_index) const
		{
			CORE_ASSERT(frame_index < command_buffers.size(), "");
			return command_buffers[frame_index];
		}

	private:
		VkCommandPool command_pool = nullptr;
		std::vector<VkCommandBuffer> command_buffers;
		VkCommandBuffer active_command_buffer = nullptr;
		std::vector<VkFence> wait_fences;

		bool owned_by_swapchain = false;
	};

} // namespace ForgottenEngine