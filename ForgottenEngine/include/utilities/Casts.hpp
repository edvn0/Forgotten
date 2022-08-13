#pragma once

struct GLFWwindow;

void* glfwGetWindowUserPointer(GLFWwindow);

namespace ForgottenEngine::Cast {

	template <typename Out, typename In> static constexpr inline Out& as(In* in) { return *static_cast<Out*>(in); }
	template <typename Out, typename In> static constexpr inline Out& as(In in) { return *static_cast<Out*>(in); }

	template <typename Out> static constexpr inline Out& as(GLFWwindow* in) { return *static_cast<Out*>(glfwGetWindowUserPointer(in)); }

} // namespace ForgottenEngine::Cast
