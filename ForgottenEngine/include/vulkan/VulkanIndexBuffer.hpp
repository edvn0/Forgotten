#pragma once

#include "Buffer.hpp"
#include "render/IndexBuffer.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	class VulkanIndexBuffer : public IndexBuffer {
	public:
		VulkanIndexBuffer(uint32_t size);
		VulkanIndexBuffer(void* data, uint32_t size = 0);
		virtual ~VulkanIndexBuffer();

		virtual void set_data(void* buffer, uint32_t size, uint32_t offset = 0) override;
		virtual void bind() const override;

		virtual uint32_t get_count() const override { return size / sizeof(uint32_t); }

		virtual uint32_t get_size() const override { return size; }
		virtual RendererID get_renderer_id() const override;

		VkBuffer get_vulkan_buffer() { return vulkan_buffer; }

	private:
		uint32_t size = 0;
		Buffer local_data;

		VkBuffer vulkan_buffer = nullptr;
		VmaAllocation memory_allocation;
	};

} // namespace ForgottenEngine