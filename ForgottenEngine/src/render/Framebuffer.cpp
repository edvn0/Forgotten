//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/Framebuffer.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace ForgottenEngine {

	Reference<Framebuffer> Framebuffer::create(const FramebufferSpecification& spec)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanFramebuffer>::create(spec);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine