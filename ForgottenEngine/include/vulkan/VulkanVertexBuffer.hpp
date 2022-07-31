#pragma once

#include "render/VertexBuffer.hpp"

#include "Buffer.hpp"

#include "vk_mem_alloc.h"

namespace ForgottenEngine {

class VulkanVertexBuffer : public VertexBuffer {
public:
	VulkanVertexBuffer(void* data, uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Static);
	VulkanVertexBuffer(uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Dynamic);

	virtual ~VulkanVertexBuffer() override;

	virtual void set_data(void* buffer, uint32_t size, uint32_t offset = 0) override;
	virtual void rt_set_data(void* buffer, uint32_t size, uint32_t offset = 0) override;
	virtual void bind() const override { }

	virtual unsigned int get_size() const override { return size; }
	virtual RendererID get_renderer_id() const override { return 0; }

	VkBuffer get_vulkan_buffer() const { return vulkan_buffer; }

private:
	uint32_t size = 0;
	Buffer local_data;

	VkBuffer vulkan_buffer = nullptr;
	VmaAllocation memory_allocation;
};

}