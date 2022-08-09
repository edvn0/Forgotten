#include "fg_pch.hpp"

#include "implementations/MacOS/MacOSWindow.hpp"

#include "events/ApplicationEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"
#include "imgui/ImGuiLayer.hpp"

#include "Application.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanSwapchain.hpp"

#include <GLFW/glfw3.h>

#include <utility>

namespace ForgottenEngine {

static bool is_glfw_initialized = false;

Window* Window::create(const ApplicationProperties& props) { return new MacOSWindow(props); };

MacOSWindow::MacOSWindow(ApplicationProperties props)
	: props(std::move(props)){};

MacOSWindow::~MacOSWindow() { shutdown(); }

void MacOSWindow::on_update(){};

void MacOSWindow::set_vsync(bool enabled)
{
	window_data.vsync = enabled;
	props.v_sync = enabled;
	Application::the().queue_event([&]() {
		swapchain.set_vsync(props.v_sync);
		swapchain.on_resize(props.width, props.height);
	});
}

bool MacOSWindow::is_vsync() { return window_data.vsync; }

void MacOSWindow::resize_window(float w, float h) const { glfwSetWindowSize(glfw_window, (int)w, (int)h); }

void MacOSWindow::resize_framebuffer(int w, int h) const
{
	(void)w;
	(void)h;
}

void MacOSWindow::init()
{
	window_data.title = props.title;
	window_data.width = props.width;
	window_data.height = props.height;

	CORE_INFO("Creating a new MacOSX window {0} ({1}, {2})", props.title, props.width, props.height);

	if (!is_glfw_initialized) {
		int success = glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

#ifdef FORGOTTEN_FULLSCREEN
		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif

		if (!success) {
			CORE_ERROR("Initialization does not work: {0}", success);
		}

		is_glfw_initialized = true;
	}

	glfw_window
		= glfwCreateWindow((int)props.width, (int)props.height, window_data.title.c_str(), nullptr, nullptr);

	glfwGetWindowContentScale(glfw_window, &pixel_size_x, &pixel_size_y);
	CORE_INFO("Pixel density: [{0} X {1}]", pixel_size_x, pixel_size_y);
	glfwSetWindowSize(glfw_window, props.width / pixel_size_x, props.height / pixel_size_y);

	bool raw_motion_support = glfwRawMouseMotionSupported();
	if (raw_motion_support)
		glfwSetInputMode(glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	else
		CORE_WARN("Platform: Raw mouse motion not supported.");

	glfwSetWindowUserPointer(glfw_window, &window_data);

	render_context = RendererContext::create();
	render_context->init();

	Reference<VulkanContext> ctxt = render_context.as<VulkanContext>();

	swapchain.init(VulkanContext::get_instance());
	swapchain.init_surface(glfw_window);

	swapchain.create(&window_data.width, &window_data.height, props.v_sync);

	int width, height;
	glfwGetWindowSize(glfw_window, &width, &height);
	window_data.width = width;
	window_data.height = height;

	set_vsync(true);
	setup_events();
};

void MacOSWindow::shutdown()
{
	swapchain.destroy();

	glfwTerminate();
	glfwDestroyWindow(glfw_window);
};

void MacOSWindow::setup_events()
{
	glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow* window, int w, int h) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

		WindowFramebufferEvent event(w, h);
		user_ptr.width = w;
		user_ptr.height = h;
		user_ptr.callback(event);
	});

	glfwSetWindowSizeCallback(glfw_window, [](GLFWwindow* window, int width, int height) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

		WindowResizeEvent event((float)width, (float)height);
		user_ptr.width = width;
		user_ptr.height = height;
		user_ptr.callback(event);
	});

	glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow* window) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		WindowCloseEvent closed;
		user_ptr.callback(closed);
	});

	glfwSetKeyCallback(glfw_window, [](GLFWwindow* window, int key, int scancode, int action, int modes) {
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

	glfwSetMouseButtonCallback(glfw_window, [](GLFWwindow* window, int button, int action, int mods) {
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

	glfwSetCharCallback(glfw_window, [](GLFWwindow* window, unsigned int c) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		auto key_code = KeyCode(c);
		KeyTypedEvent event(key_code);
		user_ptr.callback(event);
	});

	glfwSetScrollCallback(glfw_window, [](GLFWwindow* window, double xo, double yo) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		MouseScrolledEvent event((float)xo, (float)yo);
		user_ptr.callback(event);
	});

	glfwSetCursorPosCallback(glfw_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto user_ptr = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		MouseMovedEvent event((float)xpos, (float)ypos);
		user_ptr.callback(event);
	});
}

void MacOSWindow::process_events() { glfwPollEvents(); }

}
