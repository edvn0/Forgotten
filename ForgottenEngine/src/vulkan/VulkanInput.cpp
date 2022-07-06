#include "fg_pch.hpp"

#include "Input.hpp"
#include "vulkan/VulkanContext.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

bool Input::key(KeyCode key)
{
	return glfwGetKey(VulkanContext::get_window_handle(), static_cast<int>(key)) == GLFW_PRESS;
};

bool Input::mouse(MouseCode key)
{
	return glfwGetMouseButton(VulkanContext::get_window_handle(), static_cast<int>(key)) == GLFW_PRESS;
};

}