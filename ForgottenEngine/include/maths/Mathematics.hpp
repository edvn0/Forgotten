#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <type_traits>

namespace ForgottenEngine::Maths {

	template <typename T>
	concept integral = std::integral<T>;

	template <typename T>
	concept uvec = std::is_same_v<T, glm::uvec2>;

	template <typename T>
	concept ivec = std::is_same_v<T, glm::ivec2>;

	bool decompose_transform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

	inline static auto divide_and_round_up(std::integral auto dividend, std::integral auto divisor) { return (dividend + divisor - 1) / divisor; }

	inline static auto divide_and_round_up(uvec auto dividend, integral auto divisor)
	{
		return glm::uvec2 { divide_and_round_up(dividend.x, divisor), divide_and_round_up(dividend.y, divisor) };
	}

	inline static auto divide_and_round_up(ivec auto dividend, integral auto divisor)
	{
		return glm::ivec2 { divide_and_round_up(dividend.x, divisor), divide_and_round_up(dividend.y, divisor) };
	}

} // namespace ForgottenEngine::Maths