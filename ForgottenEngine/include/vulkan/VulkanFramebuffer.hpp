//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/Framebuffer.hpp"

namespace ForgottenEngine {
class VulkanFramebuffer : public Framebuffer {
public:
	explicit VulkanFramebuffer(const FramebufferSpecification& spec);
	void Bind() const override;
	void Unbind() const override;
	void Resize(uint32_t width, uint32_t height, bool forceRecreate) override;
	void AddResizeCallback(const std::function<void(Reference<Framebuffer>)>& func) override;
	void BindTexture(uint32_t attachmentIndex, uint32_t slot) const override;
	uint32_t GetWidth() const override;
	uint32_t GetHeight() const override;
	RendererID GetRendererID() const override;
	Reference<Image2D> GetImage(uint32_t attachmentIndex) const override;
	Reference<Image2D> GetDepthImage() const override;
	const FramebufferSpecification& GetSpecification() const override;
};
}