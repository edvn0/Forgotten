#include "fg_pch.hpp"

#include "Timer.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

template <typename T> T Timer::get_time() { return static_cast<T>(glfwGetTime()); }

template <> double Timer::get_time() { return static_cast<double>(glfwGetTime()); }
template <> float Timer::get_time() { return static_cast<float>(glfwGetTime()); }
template <> long double Timer::get_time() { return static_cast<long double>(glfwGetTime()); }

}