#include "fg_pch.hpp"

#include "vulkan/VulkanRenderer.hpp"

#include "render/IndexBuffer.hpp"
#include "render/RenderCommandQueue.hpp"
#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "render/RendererCapabilites.hpp"
#include "render/Texture.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanShader.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

using PerFrameWriteDescriptor = std::vector<std::vector<VkWriteDescriptorSet>>;

struct VulkanRendererData {
	RendererCapabilities RenderCaps;

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

void VulkanRenderer::init() { renderer_data = new VulkanRendererData(); };

void VulkanRenderer::shut_down(){};

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
	// TODO!!
}

void VulkanRenderer::end_render_pass(Reference<RenderCommandBuffer> command_buffer)
{
	// TODO!!
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
