//
// Created by Edwin Carlsson on 2022-07-08.
//

#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/VulkanAllocatedBuffer.hpp"
#include <functional>
#include <vulkan/vulkan.h>

namespace ForgottenEngine::MemoryMapper {

template <typename T, typename Func>
static void effect_mmap(VmaAllocator& allocator, AllocatedBuffer& buffer, Func&& effect);

static void effect_mmap(VmaAllocator& allocator, AllocatedBuffer& buffer, std::function<void(void*)>&& effect);

static void effect_mmap(
	VmaAllocator& allocator, AllocatedBuffer& buffer, const std::function<void(void*)>& effect);

}