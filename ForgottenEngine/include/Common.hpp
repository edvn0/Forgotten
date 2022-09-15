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
#include <unordered_set>
#include <vector>

template <typename T>
concept MapType = std::same_as<T,
	std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> || std::same_as<T,
	std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>> || std::
	same_as<T, std::unordered_set<typename T::key_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;

template <MapType T, typename ToFind> static constexpr bool is_in_map(const T& map, const ToFind& to_find) { return map.find(to_find) != map.end(); }

#define BIT(x) (1u << (x))

#define FORGOTTEN_ENABLE_ASSERTS
#define FORGOTTEN_ENABLE_VERIFY
#define FORGOTTEN_ENABLE_CORE_BREAK

#define _ARG2(_0, _1, _2, ...) _2
#define NARG2(...) _ARG2(__VA_ARGS__, 2, 1, 0)

#ifdef FORGOTTEN_ENABLE_ASSERTS

template <typename PositiveCondition = bool> static constexpr inline void CORE_ASSERT_BOOL(PositiveCondition&& x)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Assertion failed.");
		debug_break();
	}
}

template <typename PositiveCondition = bool, typename... T> static constexpr inline void CORE_ASSERT(PositiveCondition&& x, T&&... args)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Assertion failed. Message: {}", args...);
		debug_break();
	}
}

#else
#define CORE_ASSERT(condition, ...)
#endif

#ifdef FORGOTTEN_ENABLE_VERIFY

template <typename PositiveCondition = bool> static constexpr inline void CORE_VERIFY_BOOL(PositiveCondition&& x)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed.");
		debug_break();
	}
}

template <typename PositiveCondition = bool, typename... T> static constexpr inline void CORE_VERIFY(PositiveCondition&& x, T&&... args)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed. Message: {}", args...);
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
