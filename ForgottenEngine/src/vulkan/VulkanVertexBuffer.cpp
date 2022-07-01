//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "vulkan/VulkanVertexBuffer.hpp"

namespace ForgottenEngine {

std::string VulkanVertexBuffer::to_string() { return "VulkanVertexBuffer: {" + std::to_string(size) + "}"; };

} // ForgottenEngine