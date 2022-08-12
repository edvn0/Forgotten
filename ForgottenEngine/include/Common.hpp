//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "Logger.hpp"
#include "PlatformSpecific.hpp"
#include "Reference.hpp"
#include "serialize/Serialization.hpp"

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#define FORGOTTEN_ENABLE_ASSERTS
#define FORGOTTEN_ENABLE_VERIFY
#define FORGOTTEN_ENABLE_CORE_BREAK

#define _ARG2(_0, _1, _2, ...) _2
#define NARG2(...) _ARG2(__VA_ARGS__, 2, 1, 0)

#ifdef FORGOTTEN_ENABLE_ASSERTS

#define CORE_ASSERT_BOOL(x)                                                                                                 \
	if (!(x)) {                                                                                                             \
		::ForgottenEngine::Logger::get_core_logger()->error("Assetion failed at: file: {}, line:, {}", __FILE__, __LINE__); \
		debug_break();                                                                                                      \
	}

#define CORE_ASSERT(x, ...)                                                                                                                           \
	if (!(x)) {                                                                                                                                       \
		::ForgottenEngine::Logger::get_core_logger()->error("Assetion failed at: file: {}, line:, {}. Message: {}", __FILE__, __LINE__, __VA_ARGS__); \
		debug_break();                                                                                                                                \
	}

#else
#define CORE_ASSERT(condition, ...)
#endif

#ifdef FORGOTTEN_ENABLE_VERIFY

#define CORE_VERIFY_BOOL(x)                                                                                                \
	if (!(x)) {                                                                                                            \
		::ForgottenEngine::Logger::get_core_logger()->warn("Assetion failed at: file: {}, line:, {}", __FILE__, __LINE__); \
		debug_break();                                                                                                     \
	}

#define CORE_VERIFY(x, ...)                                                                                                                          \
	if (!(x)) {                                                                                                                                      \
		::ForgottenEngine::Logger::get_core_logger()->warn("Assetion failed at: file: {}, line:, {}. Message: {}", __FILE__, __LINE__, __VA_ARGS__); \
		debug_break();                                                                                                                               \
	}

#else
#define CORE_VERIFY(condition, ...)
#endif

namespace ForgottenEngine {

	using RendererID = uint32_t;

	typedef unsigned char byte;

#define VK_CHECK(x)                              \
	do {                                         \
		VkResult err = x;                        \
		if (err) {                               \
			CORE_ERROR("Vulkan Error: {}", err); \
			debug_break();                       \
		}                                        \
	} while (0)

}; // namespace ForgottenEngine
