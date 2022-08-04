//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanStorageBufferSet.hpp"

namespace ForgottenEngine {

VulkanStorageBufferSet::VulkanStorageBufferSet(uint32_t size) { }

void VulkanStorageBufferSet::create(uint32_t size, uint32_t binding) { }

Reference<StorageBuffer> VulkanStorageBufferSet::get_impl(uint32_t binding, uint32_t set, uint32_t frame)
{
	return {};
}

void VulkanStorageBufferSet::set_impl(Reference<StorageBuffer> buffer, uint32_t set, uint32_t frame) { }

void VulkanStorageBufferSet::resize(uint32_t binding, uint32_t set, uint32_t newSize) { }

}