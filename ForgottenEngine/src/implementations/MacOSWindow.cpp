#include "fg_pch.hpp"

#include "implementations/MacOSWindow.hpp"

#include "events/ApplicationEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"
#include "imgui/ImGuiLayer.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

static bool is_glfw_initialized = false;

Window* Window::create(const WindowProps& props) { return new MacOSWindow(props); };

MacOSWindow::MacOSWindow(const WindowProps& props) { init(props); };

MacOSWindow::~MacOSWindow() { glfwDestroyWindow(win_window); }

void MacOSWindow::on_update() { glfwPollEvents(); };

void MacOSWindow::set_vsync(bool enabled) { window_data.vsync = enabled; }

bool MacOSWindow::is_vsync() { return window_data.vsync; }

void MacOSWindow::resize_window(float w, float h) const { glfwSetWindowSize(win_window, (int)w, (int)h); }

void MacOSWindow::resize_framebuffer(int w, int h) const
{
	(void)w;
	(void)h;
}

void MacOSWindow::init(const WindowProps& props)
{
	window_data.title = props.title;
	window_data.width = props.width;
	window_data.height = props.height;

	CORE_INFO("Creating a new MacOSX window {0} ({1}, {2})", props.title, props.width, props.height);

	if (!is_glfw_initialized) {
		int success = glfwInit();

		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		if (!success) {
			CORE_ERROR("Initialization does not work: {0}", success);
		}

		is_glfw_initialized = true;
	}

	win_window
		= glfwCreateWindow((int)props.width, (int)props.height, window_data.title.c_str(), nullptr, nullptr);

	glfwGetWindowContentScale(win_window, &pixel_size_x, &pixel_size_y);
	CORE_INFO("Pixel density: [{0} X {1}]", pixel_size_x, pixel_size_y);

	glfwSetWindowUserPointer(win_window, &window_data);
	set_vsync(true);
	setup_events();
};

void MacOSWindow::shutdown() { glfwDestroyWindow(win_window); };

void MacOSWindow::setup_events()
{
	glfwSetFramebufferSizeCallback(win_window, [](GLFWwindow* window, int w, int h) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

		WindowFramebufferEvent event(w, h);
		user_ptr.width = w;
		user_ptr.height = h;
		user_ptr.callback(event);
	});

	glfwSetWindowSizeCallback(win_window, [](GLFWwindow* window, int width, int height) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

		WindowResizeEvent event((float)width, (float)height);
		user_ptr.width = width;
		user_ptr.height = height;
		user_ptr.callback(event);
	});

	glfwSetWindowCloseCallback(win_window, [](GLFWwindow* window) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		WindowCloseEvent closed;
		user_ptr.callback(closed);
	});

	glfwSetKeyCallback(win_window, [](GLFWwindow* window, int key, int scancode, int action, int modes) {
		(void)scancode;
		(void)modes;

		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

		switch (action) {
		case GLFW_PRESS: {
			KeyPressedEvent event(KeyCode(key), 0);
			user_ptr.callback(event);
			break;
		}
		case GLFW_RELEASE: {
			auto key_code = KeyCode(key);
			KeyReleasedEvent event(key_code);
			user_ptr.callback(event);
			break;
		}
		case GLFW_REPEAT: {
			KeyPressedEvent event(KeyCode(key), 1);
			user_ptr.callback(event);
			break;
		}
		}
	});

	glfwSetMouseButtonCallback(win_window, [](GLFWwindow* window, int button, int action, int mods) {
		(void)mods;

		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		switch (action) {
		case GLFW_PRESS: {
			MouseButtonPressedEvent event(button, x, y);
			user_ptr.callback(event);
			break;
		}
		case GLFW_RELEASE: {
			MouseButtonReleasedEvent event(button, x, y);
			user_ptr.callback(event);
			break;
		}
		}
	});

	glfwSetCharCallback(win_window, [](GLFWwindow* window, unsigned int c) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		auto key_code = KeyCode(c);
		KeyTypedEvent event(key_code);
		user_ptr.callback(event);
	});

	glfwSetScrollCallback(win_window, [](GLFWwindow* window, double xo, double yo) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		MouseScrolledEvent event((float)xo, (float)yo);
		user_ptr.callback(event);
	});

	glfwSetCursorPosCallback(win_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		MouseMovedEvent event((float)xpos, (float)ypos);
		user_ptr.callback(event);
	});
}

}
