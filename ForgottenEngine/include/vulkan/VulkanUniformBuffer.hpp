#pragma once

#include "render/UniformBuffer.hpp"

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

class VulkanUniformBuffer : public UniformBuffer {
public:
	VulkanUniformBuffer(uint32_t size, uint32_t binding);
	uint32_t get_binding() const override { return 0; }

	const VkDescriptorBufferInfo& get_descriptor_buffer_info() const { return descriptor_buffer_info; }

protected:
	void rt_set_data_impl(const void* data, uint32_t size, uint32_t offset) override;
	void set_data_impl(const void* data, uint32_t size, uint32_t offset) override;

private:
	VmaAllocation memory_alloc = nullptr;
	VkBuffer vk_buffer;
	VkDescriptorBufferInfo descriptor_buffer_info{};
	uint32_t size = 0;
	uint32_t binding = 0;
	std::string name;
	VkShaderStageFlagBits shader_stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

	uint8_t* local_storage = nullptr;
};

}