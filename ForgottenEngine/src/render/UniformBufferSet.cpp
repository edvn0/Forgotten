//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/UniformBufferSet.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanUniformBufferSet.hpp"

namespace ForgottenEngine {

	Reference<UniformBufferSet> UniformBufferSet::create(uint32_t size)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanUniformBufferSet>::create(size);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine