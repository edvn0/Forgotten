#include "fg_pch.hpp"

#include "Application.hpp"
#include "Input.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

	static auto* get_window() { return static_cast<GLFWwindow*>(Application::the().get_window().get_natively()); }

	bool Input::key(KeyCode key) { return glfwGetKey(get_window(), static_cast<int>(key)) == GLFW_PRESS; };

	bool Input::mouse(MouseCode key) { return glfwGetMouseButton(get_window(), static_cast<int>(key)) == GLFW_PRESS; }

	glm::vec2 Input::mouse_position()
	{
		double out_x, out_y;
		glfwGetCursorPos(get_window(), &out_x, &out_y);
		return { out_x, out_y };
	}

} // namespace ForgottenEngine