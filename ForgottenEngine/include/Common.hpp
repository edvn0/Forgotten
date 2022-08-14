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

#define BIT(x) (1u << x)

#define FORGOTTEN_ENABLE_ASSERTS
#define FORGOTTEN_ENABLE_VERIFY
#define FORGOTTEN_ENABLE_CORE_BREAK

#define _ARG2(_0, _1, _2, ...) _2
#define NARG2(...) _ARG2(__VA_ARGS__, 2, 1, 0)

#ifdef FORGOTTEN_ENABLE_ASSERTS

template <typename Condition = bool> [[noreturn]] static constexpr inline void CORE_ASSERT_BOOL(Condition&& x)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Assertion failed at: file: {}:{}", __FILE__, __LINE__);
		debug_break();
	}
}

template <typename Condition = bool, typename... T> [[noreturn]] static constexpr inline void CORE_ASSERT(Condition&& x, T&&... args...)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Assertion failed at: file: {}:{}. Message: {}", __FILE__, __LINE__, args...);
		debug_break();
	}
}

#else
#define CORE_ASSERT(condition, ...)
#endif

#ifdef FORGOTTEN_ENABLE_VERIFY

template <typename Condition = bool> [[noreturn]] static constexpr inline void CORE_VERIFY_BOOL(Condition&& x)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed at: file: {}:{}", __FILE__, __LINE__);
		debug_break();
	}
}

template <typename Condition = bool, typename... T> [[noreturn]] static constexpr inline void CORE_VERIFY(Condition&& x, T&&... args...)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed at: file: {}:{}. Message: {}", __FILE__, __LINE__, args...);
		debug_break();
	}
}

#else
#define CORE_VERIFY(condition, ...)
#endif

namespace ForgottenEngine {

	using RendererID = uint32_t;

	typedef unsigned char byte;

#define VK_CHECK(x)                                                                                                                                  \
	do {                                                                                                                                             \
		VkResult err = x;                                                                                                                            \
		if (err) {                                                                                                                                   \
			CORE_ERROR("Vulkan Error: {}", err);                                                                                                     \
			debug_break();                                                                                                                           \
		}                                                                                                                                            \
	} while (0)

}; // namespace ForgottenEngine
