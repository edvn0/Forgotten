//
// Created by Edwin Carlsson on 2022-07-26.
//

#include "render/RendererContext.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	Reference<RendererContext> RendererContext::create()
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			return Reference<VulkanContext>::create();
		}
		core_assert(false, "Incorrect rendering APi chosen.");
	}

} // namespace ForgottenEngine