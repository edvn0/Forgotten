#pragma once

#include "render/RendererAPI.hpp"

namespace ForgottenEngine {

class SceneRenderer;

class VulkanRenderer : public RendererAPI {
public:
	~VulkanRenderer() override = default;

	void init() override;
	void shut_down() override;

public:
	void begin_frame() override;

	void begin_render_pass(Reference<RenderCommandBuffer> command_buffer, Reference<RenderPass> render_pass,
		bool explicit_clear) override;

	void end_render_pass(Reference<RenderCommandBuffer> command_buffer) override;

	void end_frame() override;

public:
	// SUBMITS
	void render_geometry(Reference<RenderCommandBuffer> command_buffer, Reference<Pipeline> pipeline,
		Reference<UniformBufferSet> ubs, Reference<StorageBufferSet> sbs, Reference<Material> material,
		Reference<VertexBuffer> vb, Reference<IndexBuffer> ib, const glm::mat4& transform,
		uint32_t index_count) override;
	// END SUBMITS
public:
	static VkDescriptorSet rt_allocate_descriptor_set(VkDescriptorSetAllocateInfo info);
};

} // namespace ForgottenEngine
