//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/RenderPass.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanRenderPass.hpp"

namespace ForgottenEngine {

	Reference<RenderPass> RenderPass::create(const RenderPassSpecification& spec)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanRenderPass>::create(spec);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine