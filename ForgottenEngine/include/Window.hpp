#pragma once

#include "Common.hpp"
#include "events/Event.hpp"

#include "ApplicationProperties.hpp"

#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

class Window {
public:
	using EventCallback = std::function<void(Event&)>;

	virtual ~Window() = default;

	[[nodiscard]] virtual size_t get_width() const = 0;
	[[nodiscard]] virtual size_t get_height() const = 0;

	virtual void on_update() = 0;

	virtual void init() = 0;

	virtual void set_event_callback(const EventCallback& callback) = 0;
	virtual void set_vsync(bool enabled) = 0;
	virtual bool is_vsync() = 0;
	virtual void resize_window(float w, float h) const = 0;
	virtual void resize_framebuffer(int w, int h) const = 0;

	[[nodiscard]] virtual void* get_natively() const = 0;

	static Window* create(const ApplicationProperties& props = ApplicationProperties());
};
}
