#pragma once

#include "Window.hpp"
#include <GLFW/glfw3.h>

namespace ForgottenEngine {

class MacOSWindow : public Window {
public:
	explicit MacOSWindow(const WindowProps& props);
	~MacOSWindow() override;

	void on_update() override;

	[[nodiscard]] size_t get_width() const override { return window_data.width; };
	[[nodiscard]] size_t get_height() const override { return window_data.height; };

	void set_event_callback(const EventCallback& callback) override { window_data.callback = callback; };
	void set_vsync(bool enabled) override;
	bool is_vsync() override;
	void resize_window(float w, float h) const override;
	void resize_framebuffer(int w, int h) const override;

	[[nodiscard]] inline void* get_natively() const override { return win_window; };

private:
	virtual void init(const WindowProps& props);
	virtual void shutdown();
	virtual void setup_events();

private:
	GLFWwindow* win_window;

	struct WindowData {
		std::string title = "Window";
		size_t width = 1920;
		size_t height = 1080;
		bool vsync = false;

		EventCallback callback;
	};

	float pixel_size_x = 2.0;
	float pixel_size_y = 2.0;

	WindowData window_data;
};

}
