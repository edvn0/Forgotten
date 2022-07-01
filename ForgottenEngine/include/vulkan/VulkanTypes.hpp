//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "vk_mem_alloc.h"

namespace ForgottenEngine {

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
};

}