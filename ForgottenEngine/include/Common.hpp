//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "Logger.hpp"
#include <memory>

namespace ForgottenEngine {

template <typename T, typename Arg> std::shared_ptr<T> shared(Arg&& arg)
{
	return std::shared_ptr<T>(new T(std::forward<Arg>(arg)));
};

#define VK_CHECK(x)                                                                                               \
	do {                                                                                                          \
		VkResult err = x;                                                                                         \
		if (err) {                                                                                                \
			CORE_ERROR("Vulkan Error: {}", err);                                                                  \
			abort();                                                                                              \
		}                                                                                                         \
	} while (0)

};