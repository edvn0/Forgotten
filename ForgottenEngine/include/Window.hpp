#pragma once

#include "ApplicationProperties.hpp"
#include "Common.hpp"
#include "events/Event.hpp"
#include "render/RendererContext.hpp"

namespace ForgottenEngine {

	class VulkanSwapchain;
	class RendererContext;

	template <typename T>
	concept arithmetic = std::integral<T> or std::floating_point<T>;

	class Window {
	public:
		using EventCallback = std::function<void(Event&)>;

		virtual ~Window() = default;

		[[nodiscard]] virtual size_t get_width() const = 0;
		[[nodiscard]] virtual size_t get_height() const = 0;

		template <arithmetic T> [[nodiscard]] inline std::pair<T, T> get_size() const
		{
			return { static_cast<T>(get_width()), static_cast<T>(get_height()) };
		};

		virtual void on_update() = 0;

		virtual void init() = 0;

		virtual void set_event_callback(const EventCallback& callback) = 0;
		virtual void set_vsync(bool enabled) = 0;
		virtual void process_events() = 0;
		virtual void set_title(const std::string new_title) = 0;
		virtual bool is_vsync() = 0;
		virtual void resize_window(float w, float h) const = 0;
		virtual void resize_framebuffer(int w, int h) const = 0;

		[[nodiscard]] virtual void* get_natively() const = 0;

		virtual Reference<RendererContext> get_context() = 0;
		virtual VulkanSwapchain& get_swapchain() = 0;
		virtual void swap_buffers() = 0;

		static Window* create(const ApplicationProperties& props = ApplicationProperties());
	};
} // namespace ForgottenEngine
