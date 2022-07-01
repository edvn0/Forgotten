//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "VertexBuffer.hpp"
#include "fg_pch.hpp"

#include "vulkan/VulkanVertexBuffer.hpp"

namespace ForgottenEngine {

std::shared_ptr<VertexBuffer> VertexBuffer::create(size_t size)
{
	return std::make_shared<VulkanVertexBuffer>(size);
}

}