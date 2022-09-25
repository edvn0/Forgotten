//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/Framebuffer.hpp"
#include "VulkanRenderPass.hpp"

namespace ForgottenEngine {

	class VulkanFramebuffer : public Framebuffer {
	public:
		explicit VulkanFramebuffer(const FramebufferSpecification& spec);
		~VulkanFramebuffer() override;

		void bind() const override {};
		void unbind() const override {};
		void resize(uint32_t width, uint32_t height, bool force_recreate) override;
		void add_resize_callback(const std::function<void(Reference<Framebuffer>)>& func) override;
		void bind_texture(uint32_t attachment_index, uint32_t slot) const override {};
		uint32_t get_width() const override { return width; };
		uint32_t get_height() const override { return height; };
		RendererID get_renderer_id() const override { return renderer_id; };

		Reference<Image2D> get_image(uint32_t attachment_index) const override { return attachment_images[attachment_index]; };
		Reference<Image2D> get_depth_image() const override { return depth_image; };

		VkRenderPass get_render_pass() const { return render_pass; }
		VkFramebuffer get_vulkan_framebuffer() const { return framebuffer; }
		const std::vector<VkClearValue>& get_vulkan_clear_values() const { return clear_values; }
		size_t get_color_attachment_count() const { return spec.swapchain_target ? 1 : attachment_images.size(); }
		bool has_depth_attachment() const { return (bool)depth_image; }

		const FramebufferSpecification& get_specification() const override { return spec; }

		void release();
		void invalidate();
		void rt_invalidate();

	private:
		uint32_t width;
		uint32_t height;

		uint32_t renderer_id = 0;

		FramebufferSpecification spec;
		VkRenderPass render_pass { nullptr };
		VkFramebuffer framebuffer { nullptr };
		std::vector<VkClearValue> clear_values;

		std::vector<Reference<Image2D>> attachment_images;
		Reference<Image2D> depth_image;

		std::vector<std::function<void(Reference<Framebuffer>)>> resize_callbacks;
	};

} // namespace ForgottenEngine