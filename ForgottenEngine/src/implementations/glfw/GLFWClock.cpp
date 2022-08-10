#include "fg_pch.hpp"

#include "Clock.hpp"
#include "TimeStep.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

	template <typename T>
	T Clock::get_time() { return static_cast<T>(glfwGetTime()); }

	template <>
	double Clock::get_time() { return static_cast<double>(glfwGetTime()); }
	template <>
	float Clock::get_time() { return static_cast<float>(glfwGetTime()); }
	template <>
	long double Clock::get_time() { return static_cast<long double>(glfwGetTime()); }

} // namespace ForgottenEngine