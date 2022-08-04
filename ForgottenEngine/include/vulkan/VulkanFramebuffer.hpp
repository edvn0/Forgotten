//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/Framebuffer.hpp"

namespace ForgottenEngine {
class VulkanFramebuffer : public Framebuffer {
public:
	explicit VulkanFramebuffer(const FramebufferSpecification& spec);

	void bind() const override;
	void unbind() const override;
	void resize(uint32_t width, uint32_t height, bool forceRecreate) override;
	void add_resize_callback(const std::function<void(Reference<Framebuffer>)>& func) override;
	void bind_texture(uint32_t attachmentIndex, uint32_t slot) const override;
	uint32_t get_width() const override;
	uint32_t get_height() const override;
	RendererID get_renderer_id() const override;
	Reference<Image2D> get_image(uint32_t attachmentIndex) const override;
	Reference<Image2D> get_depth_image() const override;

	const FramebufferSpecification& get_specification() const override { return spec; }

private:
	FramebufferSpecification spec;
};

}