//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/UniformBuffer.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanUniformBuffer.hpp"

namespace ForgottenEngine {

	Reference<UniformBuffer> UniformBuffer::create(uint32_t size, uint32_t binding)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanUniformBuffer>::create(size, binding);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine