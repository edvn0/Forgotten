#pragma once

#include "render/StorageBuffer.hpp"
#include "vulkan/VulkanAllocator.hpp"

namespace ForgottenEngine {

	class VulkanStorageBuffer : public StorageBuffer {
	public:
		VulkanStorageBuffer(uint32_t size, uint32_t binding);
		virtual ~VulkanStorageBuffer() override;

		virtual uint32_t get_binding() const override { return binding; }
		virtual void resize(uint32_t newSize) override;

		const VkDescriptorBufferInfo& get_descriptor_buffer_info() const { return descriptor_info; }

	protected:
		virtual void set_data_impl(const void* data, uint32_t size, uint32_t offset) override;
		virtual void rt_set_data_impl(const void* data, uint32_t size, uint32_t offset) override;

	private:
		void release();
		void rt_invalidate();

	private:
		VmaAllocation memory_alloc = nullptr;
		VkBuffer buffer {};
		VkDescriptorBufferInfo descriptor_info {};
		uint32_t size = 0;
		uint32_t binding = 0;
		std::string name;
		VkShaderStageFlagBits shader_stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		uint8_t* local_storage = nullptr;
	};
} // namespace ForgottenEngine
