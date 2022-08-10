#pragma once

#include "Reference.hpp"
#include "Window.hpp"
#include "render/RendererContext.hpp"
#include "vulkan/VulkanSwapchain.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

	class MacOSWindow : public Window {
	public:
		explicit MacOSWindow(ApplicationProperties props);
		~MacOSWindow() override;

		void on_update() override;

		[[nodiscard]] size_t get_width() const override { return window_data.width; };
		[[nodiscard]] size_t get_height() const override { return window_data.height; };

		void init() override;

		void set_event_callback(const EventCallback& callback) override { window_data.callback = callback; };
		void set_vsync(bool enabled) override;
		void process_events() override;
		bool is_vsync() override;
		void resize_window(float w, float h) const override;
		void resize_framebuffer(int w, int h) const override;

		[[nodiscard]] inline void* get_natively() const override { return glfw_window; };

		virtual Reference<RendererContext> get_context() override { return render_context; }
		virtual VulkanSwapchain& get_swapchain() override { return swapchain; }
		virtual void swap_buffers() override { swapchain.present(); };

	private:
		virtual void shutdown();
		virtual void setup_events();

	private:
		GLFWwindow* glfw_window;
		ApplicationProperties props;
		VulkanSwapchain swapchain;
		Reference<RendererContext> render_context;

		struct WindowData {
			std::string title = "Window";
			uint32_t width = 1920;
			uint32_t height = 1080;
			bool vsync = false;

			EventCallback callback;
		};

		float pixel_size_x = 2.0;
		float pixel_size_y = 2.0;

		WindowData window_data;
	};

} // namespace ForgottenEngine
