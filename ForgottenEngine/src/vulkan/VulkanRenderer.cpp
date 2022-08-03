#include "fg_pch.hpp"

#include "vulkan/VulkanRenderer.hpp"

#include "imgui.h"

#include "render/IndexBuffer.hpp"
#include "render/VertexBuffer.hpp"

#include "render/RenderCommandQueue.hpp"
#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "render/RendererCapabilites.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBuffer.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanIndexBuffer.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanRenderCommandBuffer.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

static const char* vulkan_vendor_to_identifier_string(uint32_t vendorID)
{
	switch (vendorID) {
	case 0x10DE:
		return "NVIDIA";
	case 0x1002:
		return "AMD";
	case 0x8086:
		return "INTEL";
	case 0x13B5:
		return "ARM";
	}
	return "Unknown";
}

using PerFrameWriteDescriptor = std::vector<std::vector<VkWriteDescriptorSet>>;

struct VulkanRendererData {
	RendererCapabilities render_caps;

	Reference<Texture2D> BRDFLut;

	Reference<VertexBuffer> QuadVertexBuffer;
	Reference<IndexBuffer> QuadIndexBuffer;
	VulkanShader::ShaderMaterialDescriptorSet QuadDescriptorSet;

	std::unordered_map<SceneRenderer*, std::vector<VulkanShader::ShaderMaterialDescriptorSet>>
		RendererDescriptorSet;
	VkDescriptorSet active_descriptor_set = nullptr;
	std::vector<VkDescriptorPool> descriptor_pools;
	std::vector<uint32_t> descriptor_pool_allocation_count;

	// UniformBufferSet -> Shader Hash -> Frame -> WriteDescriptor
	std::unordered_map<UniformBufferSet*, std::unordered_map<uint64_t, PerFrameWriteDescriptor>>
		uniform_buffer_write_descriptor_cache;
	std::unordered_map<StorageBufferSet*, std::unordered_map<uint64_t, PerFrameWriteDescriptor>>
		storage_buffer_write_descriptor_cache;

	// Default samplers
	VkSampler clamp_sampler = nullptr;
	VkSampler point_sampler = nullptr;

	int32_t selected_draw_call = -1;
	int32_t draw_call_count = 0;
};

static VulkanRendererData* renderer_data = nullptr;

static inline VulkanRendererData& the()
{
	CORE_ASSERT(renderer_data, "");
	return *renderer_data;
}

static RenderCommandQueue* command_queue = nullptr;

static const std::vector<std::vector<VkWriteDescriptorSet>>&
rt_retrieve_or_create_uniform_buffer_write_descriptors(
	Reference<UniformBufferSet> uniformBufferSet, Reference<VulkanMaterial> vulkanMaterial)
{
	size_t shaderHash = vulkanMaterial->get_shader()->get_hash();
	if (renderer_data->uniform_buffer_write_descriptor_cache.find(uniformBufferSet.raw())
		!= renderer_data->uniform_buffer_write_descriptor_cache.end()) {
		const auto& shaderMap = renderer_data->uniform_buffer_write_descriptor_cache.at(uniformBufferSet.raw());
		if (shaderMap.find(shaderHash) != shaderMap.end()) {
			const auto& writeDescriptors = shaderMap.at(shaderHash);
			return writeDescriptors;
		}
	}

	uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
	Reference<VulkanShader> vulkanShader = vulkanMaterial->get_shader().as<VulkanShader>();
	if (vulkanShader->has_descriptor_set(0)) {
		const auto& shaderDescriptorSets = vulkanShader->get_shader_descriptor_sets();
		if (!shaderDescriptorSets.empty()) {
			for (auto&& [binding, shaderUB] : shaderDescriptorSets[0].uniform_buffers) {
				auto& writeDescriptors
					= renderer_data->uniform_buffer_write_descriptor_cache[uniformBufferSet.raw()][shaderHash];
				writeDescriptors.resize(frames_in_flight);
				for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
					Reference<VulkanUniformBuffer> uniformBuffer
						= uniformBufferSet->get(binding, 0, frame); // set = 0 for now

					VkWriteDescriptorSet writeDescriptorSet = {};
					writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					writeDescriptorSet.pBufferInfo = &uniformBuffer->get_descriptor_buffer_info();
					writeDescriptorSet.dstBinding = uniformBuffer->get_binding();
					writeDescriptors[frame].push_back(writeDescriptorSet);
				}
			}
		}
	}

	return renderer_data->uniform_buffer_write_descriptor_cache[uniformBufferSet.raw()][shaderHash];
}

static const std::vector<std::vector<VkWriteDescriptorSet>>&
rt_retrieve_or_create_storage_buffer_write_descriptors(
	Reference<StorageBufferSet> storageBufferSet, Reference<VulkanMaterial> vulkanMaterial)
{
	size_t shaderHash = vulkanMaterial->get_shader()->get_hash();
	if (renderer_data->storage_buffer_write_descriptor_cache.find(storageBufferSet.raw())
		!= renderer_data->storage_buffer_write_descriptor_cache.end()) {
		const auto& shaderMap = renderer_data->storage_buffer_write_descriptor_cache.at(storageBufferSet.raw());
		if (shaderMap.find(shaderHash) != shaderMap.end()) {
			const auto& writeDescriptors = shaderMap.at(shaderHash);
			return writeDescriptors;
		}
	}

	uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
	Reference<VulkanShader> vulkanShader = vulkanMaterial->get_shader().as<VulkanShader>();
	if (vulkanShader->has_descriptor_set(0)) {
		const auto& shaderDescriptorSets = vulkanShader->get_shader_descriptor_sets();
		if (!shaderDescriptorSets.empty()) {
			for (auto&& [binding, shaderSB] : shaderDescriptorSets[0].StorageBuffers) {
				auto& writeDescriptors
					= renderer_data->storage_buffer_write_descriptor_cache[storageBufferSet.raw()][shaderHash];
				writeDescriptors.resize(frames_in_flight);
				for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
					Reference<VulkanStorageBuffer> storageBuffer
						= storageBufferSet->get(binding, 0, frame); // set = 0 for now

					VkWriteDescriptorSet writeDescriptorSet = {};
					writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					writeDescriptorSet.pBufferInfo = &storageBuffer->get_descriptor_buffer_info();
					writeDescriptorSet.dstBinding = storageBuffer->get_binding();
					writeDescriptors[frame].push_back(writeDescriptorSet);
				}
			}
		}
	}

	return renderer_data->storage_buffer_write_descriptor_cache[storageBufferSet.raw()][shaderHash];
}

void VulkanRenderer::init()
{
	renderer_data = new VulkanRendererData();

	const auto& config = Renderer::get_config();
	renderer_data->descriptor_pools.resize(config.frames_in_flight);
	renderer_data->descriptor_pool_allocation_count.resize(config.frames_in_flight);

	auto& caps = renderer_data->render_caps;
	auto& properties = VulkanContext::get_current_device()->get_physical_device()->get_properties();
	caps.vendor = vulkan_vendor_to_identifier_string(properties.vendorID);
	caps.device = properties.deviceName;
	caps.version = std::to_string(properties.driverVersion);

	Renderer::submit([]() mutable {
		// Create Descriptor Pool
		VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 100000;
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
		for (uint32_t i = 0; i < frames_in_flight; i++) {
			VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &renderer_data->descriptor_pools[i]));
			renderer_data->descriptor_pool_allocation_count[i] = 0;
		}
	});

	// Create fullscreen quad
	float x = -1;
	float y = -1;
	float width = 2, height = 2;
	struct QuadVertex {
		glm::vec3 Position;
		glm::vec2 TexCoord;
	};

	QuadVertex* data = new QuadVertex[4];

	data[0].Position = glm::vec3(x, y, 0.0f);
	data[0].TexCoord = glm::vec2(0, 0);

	data[1].Position = glm::vec3(x + width, y, 0.0f);
	data[1].TexCoord = glm::vec2(1, 0);

	data[2].Position = glm::vec3(x + width, y + height, 0.0f);
	data[2].TexCoord = glm::vec2(1, 1);

	data[3].Position = glm::vec3(x, y + height, 0.0f);
	data[3].TexCoord = glm::vec2(0, 1);

	renderer_data->QuadVertexBuffer = VertexBuffer::create(data, 4 * sizeof(QuadVertex));
	uint32_t indices[6] = {
		0,
		1,
		2,
		2,
		3,
		0,
	};
	renderer_data->QuadIndexBuffer = IndexBuffer::create(indices, 6 * sizeof(uint32_t));
};

void VulkanRenderer::shut_down()
{
	auto device = VulkanContext::get_current_device()->get_vulkan_device();
	vkDeviceWaitIdle(device);

	delete renderer_data;
};

void VulkanRenderer::begin_frame(){};

void VulkanRenderer::end_frame() { }

void VulkanRenderer::begin_render_pass(
	Reference<RenderCommandBuffer> command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
{
	// TODO!!!!
}

void VulkanRenderer::render_geometry(Reference<RenderCommandBuffer> command_buffer, Reference<Pipeline> pipeline,
	Reference<UniformBufferSet> ubs, Reference<StorageBufferSet> sbs, Reference<Material> material,
	Reference<VertexBuffer> vb, Reference<IndexBuffer> ib, const glm::mat4& transform, uint32_t index_count)
{
	Reference<VulkanMaterial> vulkan_material = material.as<VulkanMaterial>();
	if (index_count == 0)
		index_count = ib->get_count();

	Renderer::submit([command_buffer, pipeline, ubs, vulkan_material, vb, ib, transform, index_count]() mutable {
		uint32_t frameIndex = Renderer::get_current_frame_index();
		VkCommandBuffer render_command_buffer
			= command_buffer.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

		Reference<VulkanPipeline> vulkan_pipeline = pipeline.as<VulkanPipeline>();

		VkPipelineLayout layout = vulkan_pipeline->get_vulkan_pipeline_layout();

		auto vulkan_mesh_vb = vb.as<VulkanVertexBuffer>();
		VkBuffer vb_mesh_buffer = vulkan_mesh_vb->get_vulkan_buffer();
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(render_command_buffer, 0, 1, &vb_mesh_buffer, offsets);

		auto vulkan_mesh_ib = ib.as<VulkanIndexBuffer>();
		VkBuffer ib_mesh_buffer = vulkan_mesh_ib->get_vulkan_buffer();
		vkCmdBindIndexBuffer(render_command_buffer, ib_mesh_buffer, 0, VK_INDEX_TYPE_UINT32);

		VkPipeline vk_pipeline = vulkan_pipeline->get_vulkan_pipeline();
		vkCmdBindPipeline(render_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

		const auto& write_descriptors
			= rt_retrieve_or_create_uniform_buffer_write_descriptors(ubs, vulkan_material);
		vulkan_material->rt_update_for_rendering(write_descriptors);

		uint32_t buffer_index = Renderer::get_current_frame_index();
		VkDescriptorSet descriptor_set = vulkan_material->get_descriptor_set(buffer_index);
		if (descriptor_set)
			vkCmdBindDescriptorSets(
				render_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor_set, 0, nullptr);

		vkCmdPushConstants(
			render_command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
		Buffer uniform_storage_buffers = vulkan_material->get_uniform_storage_buffer();
		if (uniform_storage_buffers)
			vkCmdPushConstants(render_command_buffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4),
				uniform_storage_buffers.size, uniform_storage_buffers.data);

		vkCmdDrawIndexed(render_command_buffer, index_count, 1, 0, 0, 0);
	});
}

void VulkanRenderer::end_render_pass(Reference<RenderCommandBuffer> command_buffer)
{
	Renderer::submit([rcb = command_buffer]() {
		uint32_t frameIndex = Renderer::get_current_frame_index();
		VkCommandBuffer commandBuffer = rcb.as<VulkanRenderCommandBuffer>()->get_active_command_buffer();

		vkCmdEndRenderPass(commandBuffer);
	});
}

void VulkanRenderer::rt_update_material_for_rendering(Reference<VulkanMaterial> material,
	Reference<UniformBufferSet> uniformBufferSet, Reference<StorageBufferSet> storageBufferSet)
{
	if (uniformBufferSet) {
		auto writeDescriptors = rt_retrieve_or_create_uniform_buffer_write_descriptors(uniformBufferSet, material);
		if (storageBufferSet) {
			const auto& storageBufferWriteDescriptors
				= rt_retrieve_or_create_storage_buffer_write_descriptors(storageBufferSet, material);

			const uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
			for (uint32_t frame = 0; frame < frames_in_flight; frame++) {
				writeDescriptors[frame].reserve(
					writeDescriptors[frame].size() + storageBufferWriteDescriptors[frame].size());
				writeDescriptors[frame].insert(writeDescriptors[frame].end(),
					storageBufferWriteDescriptors[frame].begin(), storageBufferWriteDescriptors[frame].end());
			}
		}
		material->rt_update_for_rendering(writeDescriptors);
	} else {
		material->rt_update_for_rendering();
	}
}

VkDescriptorSet VulkanRenderer::rt_allocate_descriptor_set(VkDescriptorSetAllocateInfo alloc_info)
{
	uint32_t buffer_index = Renderer::get_current_frame_index();
	alloc_info.descriptorPool = the().descriptor_pools[buffer_index];
	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
	VkDescriptorSet result;
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &result));
	the().descriptor_pool_allocation_count[buffer_index] += alloc_info.descriptorSetCount;
	return result;
}

};
