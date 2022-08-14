#pragma once

extern "C" {
struct GLFWwindow;
void* glfwGetWindowUserPointer(GLFWwindow*);
}

namespace ForgottenEngine {

	class Event;

};

namespace ForgottenEngine::Cast {

	template <typename Out, typename In> static constexpr inline Out& as(In* in) { return *static_cast<Out*>(in); }
	template <typename Out, typename In> static constexpr inline Out& as(In in) { return *static_cast<Out*>(&in); }

	template <typename Out> static constexpr inline Out& as(GLFWwindow* in) { return *static_cast<Out*>(glfwGetWindowUserPointer(in)); }
	template <typename Out> static constexpr inline Out& as(Event* in) { return *static_cast<Out*>(in); }

} // namespace ForgottenEngine::Cast
