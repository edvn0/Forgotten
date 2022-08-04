//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanFramebuffer.hpp"

namespace ForgottenEngine {

VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec)
	: spec(spec)
{
}

void VulkanFramebuffer::bind() const { }
void VulkanFramebuffer::unbind() const { }
void VulkanFramebuffer::resize(uint32_t width, uint32_t height, bool forceRecreate) { }
void VulkanFramebuffer::add_resize_callback(const std::function<void(Reference<Framebuffer>)>& func) { }
void VulkanFramebuffer::bind_texture(uint32_t attachmentIndex, uint32_t slot) const { }
uint32_t VulkanFramebuffer::get_width() const { return 0; }
uint32_t VulkanFramebuffer::get_height() const { return 0; }
RendererID VulkanFramebuffer::get_renderer_id() const { return 0; }
Reference<Image2D> VulkanFramebuffer::get_image(uint32_t attachmentIndex) const { return Reference<Image2D>(); }
Reference<Image2D> VulkanFramebuffer::get_depth_image() const { return Reference<Image2D>(); }

}