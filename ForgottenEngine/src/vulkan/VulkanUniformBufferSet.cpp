//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanUniformBufferSet.hpp"

namespace ForgottenEngine {

VulkanUniformBufferSet::VulkanUniformBufferSet(uint32_t size)
	: frames(size)
{
}

void VulkanUniformBufferSet::create(uint32_t size, uint32_t binding)
{
	for (uint32_t frame = 0; frame < frames; frame++) {
		Reference<UniformBuffer> uniformBuffer = UniformBuffer::create(size, binding);
		set(uniformBuffer, 0, frame);
	}
}

Reference<UniformBuffer> VulkanUniformBufferSet::get(uint32_t binding, uint32_t set, uint32_t frame)
{
	return frame_ubs.at(frame).at(set).at(binding);
}

void VulkanUniformBufferSet::set(const Reference<UniformBuffer>& buffer, uint32_t set, uint32_t frame)
{
	frame_ubs[frame][set][buffer->get_binding()] = buffer;
}
}