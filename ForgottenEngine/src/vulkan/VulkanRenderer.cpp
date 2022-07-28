#include "fg_pch.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanRenderer.hpp"

#include "render/RenderCommandQueue.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

struct VulkanRendererData {
	// RendererCapabilities RenderCaps;

	// Ref<Texture2D> BRDFLut;

	// Ref<VertexBuffer> QuadVertexBuffer;
	// Ref<IndexBuffer> QuadIndexBuffer;
	// VulkanShader::ShaderMaterialDescriptorSet QuadDescriptorSet;

	// std::unordered_map<SceneRenderer*, std::vector<VulkanShader::ShaderMaterialDescriptorSet>>
	// RendererDescriptorSet;
	VkDescriptorSet active_descriptor_set = nullptr;
	std::vector<VkDescriptorPool> descriptor_pools;
	std::vector<uint32_t> descriptor_pool_allocation_count;

	// UniformBufferSet -> Shader Hash -> Frame -> WriteDescriptor
	// std::unordered_map<UniformBufferSet*, std::unordered_map<uint64_t,
	// std::vector<std::vector<VkWriteDescriptorSet>>>> UniformBufferWriteDescriptorCache;
	// std::unordered_map<StorageBufferSet*, std::unordered_map<uint64_t,
	// std::vector<std::vector<VkWriteDescriptorSet>>>> StorageBufferWriteDescriptorCache;

	// Default samplers
	VkSampler clamp_sampler = nullptr;
	VkSampler point_sampler = nullptr;

	int32_t selected_draw_call = -1;
	int32_t draw_call_count = 0;
};

static VulkanRendererData* renderer_data = nullptr;

static inline VulkanRendererData& the() { return *renderer_data; }

static RenderCommandQueue* command_queue = nullptr;

void VulkanRenderer::init() { renderer_data = new VulkanRendererData(); };

void VulkanRenderer::shut_down(){};

void VulkanRenderer::begin_frame(){};

void VulkanRenderer::end_frame(){};

}
