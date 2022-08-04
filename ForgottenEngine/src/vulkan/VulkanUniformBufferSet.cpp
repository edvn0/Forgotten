//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanUniformBufferSet.hpp"

namespace ForgottenEngine {

VulkanUniformBufferSet::VulkanUniformBufferSet(uint32_t size) { }

void VulkanUniformBufferSet::create(uint32_t size, uint32_t binding) { }

Reference<UniformBuffer> VulkanUniformBufferSet::get_impl(uint32_t binding, uint32_t set, uint32_t frame)
{
	return {};
}

void VulkanUniformBufferSet::set_impl(Reference<UniformBuffer> buffer, uint32_t set, uint32_t frame) { }

}