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
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename T>
using MapTypeThird = std::unordered_map < typename T::key_type,
	  typename T::mapped_type, typename T::key_compare, typename T::allocator_type >> ;

template <typename T>
using MapTypeSecond
	= std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>;

template <typename T>
using MapTypeFirst = std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>;

template <typename T>
using MapTypeFourth = std::unordered_set<typename T::key_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>;

template <typename T>
concept MapType
	= std::same_as<T, MapTypeFirst<T>> || std::same_as<T, MapTypeSecond<T>> || std::same_as<T, MapTypeThird<T>> || std::same_as<T, MapTypeFourth<T>>;

template <MapType T, typename ToFind> static inline constexpr bool is_in_map(const T& map, const ToFind& to_find)
{
	return map.find(to_find) != map.end();
}

#define BIT(x) (1u << (x))

#define FORGOTTEN_ENABLE_ASSERTS
#define FORGOTTEN_ENABLE_VERIFY
#define FORGOTTEN_ENABLE_CORE_BREAK

#define _ARG2(_0, _1, _2, ...) _2
#define NARG2(...) _ARG2(__VA_ARGS__, 2, 1, 0)

#ifdef FORGOTTEN_ENABLE_ASSERTS

template <typename PositiveCondition = bool> static constexpr inline void core_assert_bool(PositiveCondition&& x)
{
	if (!(x)) {
		ForgottenEngine::Logger::get_core_logger()->error("Assertion failed.");
		debug_break();
	}
}

template <typename PositiveCondition = bool, typename... T>
static constexpr inline void core_assert(PositiveCondition&& x, std::string_view format, T&&... args)
{
	if (!(x)) {
		auto msg = fmt::format(format, args...);
		ForgottenEngine::Logger::get_core_logger()->error("Assertion failed. Message: {}", msg);
		debug_break();
	}
}

#else
#define core_assert(condition, ...)
#endif

#ifdef FORGOTTEN_ENABLE_VERIFY

template <typename PositiveCondition = bool> static constexpr inline void core_verify_bool(PositiveCondition&& x)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed.");
		debug_break();
	}
}

template <typename PositiveCondition = bool, typename... T> static constexpr inline void core_verify(PositiveCondition&& x, T&&... args)
{
	if (!(x)) {
		::ForgottenEngine::Logger::get_core_logger()->error("Verification failed. Message: {}", args...);
		debug_break();
	}
}

#else
#define core_verify(condition, ...)
#endif

namespace ForgottenEngine {

	using RendererID = uint32_t;

	typedef unsigned char byte;

	template <typename VulkanResult> static constexpr void vk_check(VulkanResult result)
	{
		VulkanResult err = result;
		if (err) {
			CORE_ERROR("Vulkan Error: {}", err);
			debug_break();
		}
	}

}; // namespace ForgottenEngine
