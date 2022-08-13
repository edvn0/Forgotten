#pragma once

#include "serialize/StreamReader.hpp"
#include "serialize/StreamWriter.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/vulkan.h"

#include <string>
#include <unordered_map>

namespace ForgottenEngine::ShaderResource {

	struct UniformBuffer {
		VkDescriptorBufferInfo Descriptor;
		uint32_t Size = 0;
		uint32_t BindingPoint = 0;
		std::string Name;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void serialize(StreamWriter* serializer, const UniformBuffer& instance)
		{
			serializer->write_raw(instance.Descriptor);
			serializer->write_raw(instance.Size);
			serializer->write_raw(instance.BindingPoint);
			serializer->write_string(instance.Name);
			serializer->write_raw(instance.ShaderStage);
		}

		static void deserialize(StreamReader* deserializer, UniformBuffer& instance)
		{
			deserializer->read_raw(instance.Descriptor);
			deserializer->read_raw(instance.Size);
			deserializer->read_raw(instance.BindingPoint);
			deserializer->read_string(instance.Name);
			deserializer->read_raw(instance.ShaderStage);
		}
	};

	struct StorageBuffer {
		VmaAllocation MemoryAlloc = nullptr;
		VkDescriptorBufferInfo Descriptor;
		uint32_t Size = 0;
		uint32_t BindingPoint = 0;
		std::string Name;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void serialize(StreamWriter* serializer, const StorageBuffer& instance)
		{
			serializer->write_raw(instance.Descriptor);
			serializer->write_raw(instance.Size);
			serializer->write_raw(instance.BindingPoint);
			serializer->write_string(instance.Name);
			serializer->write_raw(instance.ShaderStage);
		}

		static void deserialize(StreamReader* deserializer, StorageBuffer& instance)
		{
			deserializer->read_raw(instance.Descriptor);
			deserializer->read_raw(instance.Size);
			deserializer->read_raw(instance.BindingPoint);
			deserializer->read_string(instance.Name);
			deserializer->read_raw(instance.ShaderStage);
		}
	};

	struct ImageSampler {
		uint32_t BindingPoint = 0;
		uint32_t DescriptorSet = 0;
		uint32_t ArraySize = 0;
		std::string Name;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void serialize(StreamWriter* serializer, const ImageSampler& instance)
		{
			serializer->write_raw(instance.BindingPoint);
			serializer->write_raw(instance.DescriptorSet);
			serializer->write_raw(instance.ArraySize);
			serializer->write_string(instance.Name);
			serializer->write_raw(instance.ShaderStage);
		}

		static void deserialize(StreamReader* deserializer, ImageSampler& instance)
		{
			deserializer->read_raw(instance.BindingPoint);
			deserializer->read_raw(instance.DescriptorSet);
			deserializer->read_raw(instance.ArraySize);
			deserializer->read_string(instance.Name);
			deserializer->read_raw(instance.ShaderStage);
		}
	};

	struct PushConstantRange {
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		uint32_t Offset = 0;
		uint32_t Size = 0;

		static void serialize(StreamWriter* writer, const PushConstantRange& range) { writer->write_raw(range); }
		static void deserialize(StreamReader* reader, PushConstantRange& range) { reader->read_raw(range); }
	};

	struct ShaderDescriptorSet {
		std::unordered_map<uint32_t, UniformBuffer> uniform_buffers;
		std::unordered_map<uint32_t, StorageBuffer> storage_buffers;
		std::unordered_map<uint32_t, ImageSampler> image_samplers;
		std::unordered_map<uint32_t, ImageSampler> storage_images;
		std::unordered_map<uint32_t, ImageSampler> separate_textures; // Not really an image sampler.
		std::unordered_map<uint32_t, ImageSampler> separate_samplers;

		std::unordered_map<std::string, VkWriteDescriptorSet> write_descriptor_sets;

		operator bool() const { return !(storage_buffers.empty() && uniform_buffers.empty() && image_samplers.empty() && storage_images.empty()); }
	};

} // namespace ForgottenEngine::ShaderResource
