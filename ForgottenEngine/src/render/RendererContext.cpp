//
// Created by Edwin Carlsson on 2022-07-26.
//

#include "render/RendererContext.hpp"

#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

Reference<RendererContext> RendererContext::create() { return Reference<VulkanContext>::create(); }

}