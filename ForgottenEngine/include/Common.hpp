//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "Logger.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ForgottenEngine {

#define VK_CHECK(x)                                                                                               \
	do {                                                                                                          \
		VkResult err = x;                                                                                         \
		if (err) {                                                                                                \
			CORE_ERROR("Vulkan Error: {}", err);                                                                  \
			abort();                                                                                              \
		}                                                                                                         \
	} while (0)

};