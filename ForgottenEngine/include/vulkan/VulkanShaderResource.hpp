#pragma once

#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/vulkan.h"

#include <unordered_map>

#include <string>

namespace ForgottenEngine::ShaderResource {

struct UniformBuffer {
	VkDescriptorBufferInfo Descriptor;
	uint32_t Size = 0;
	uint32_t BindingPoint = 0;
	std::string Name;
	VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};

struct StorageBuffer {
	VmaAllocation MemoryAlloc = nullptr;
	VkDescriptorBufferInfo Descriptor;
	uint32_t Size = 0;
	uint32_t BindingPoint = 0;
	std::string Name;
	VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};

struct ImageSampler {
	uint32_t BindingPoint = 0;
	uint32_t DescriptorSet = 0;
	uint32_t ArraySize = 0;
	std::string Name;
	VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};

struct PushConstantRange {
	VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	uint32_t Offset = 0;
	uint32_t Size = 0;
};

struct ShaderDescriptorSet {
	std::unordered_map<uint32_t, UniformBuffer> uniform_buffers;
	std::unordered_map<uint32_t, StorageBuffer> storage_buffers;
	std::unordered_map<uint32_t, ImageSampler> image_samplers;
	std::unordered_map<uint32_t, ImageSampler> storage_images;
	std::unordered_map<uint32_t, ImageSampler> separate_textures; // Not really an image sampler.
	std::unordered_map<uint32_t, ImageSampler> separate_samplers;

	std::unordered_map<std::string, VkWriteDescriptorSet> write_descriptor_sets;

	operator bool() const
	{
		return !(storage_buffers.empty() && uniform_buffers.empty() && image_samplers.empty()
			&& storage_images.empty());
	}
};

}
