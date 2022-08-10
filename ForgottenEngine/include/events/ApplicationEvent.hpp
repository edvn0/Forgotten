#pragma once

#include "events/Event.hpp"

namespace ForgottenEngine {

	class WindowFramebufferEvent : public Event {
	public:
		WindowFramebufferEvent(int w, int h)
			: width(w)
			, height(h)
		{
		}

		[[nodiscard]] inline int get_width() const { return width; }
		[[nodiscard]] inline int get_height() const { return height; }

		static EventType get_static_type() { return EventType::WindowFramebuffer; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "WindowFramebuffer"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };

	private:
		int width;
		int height;
	};

	class WindowResizeEvent : public Event {
	public:
		WindowResizeEvent(float width, float height)
			: width(width)
			, height(height)
		{
		}

		[[nodiscard]] inline float get_width() const { return width; }
		[[nodiscard]] inline float get_height() const { return height; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << width << ", " << height;
			return ss.str();
		}

		static EventType get_static_type() { return EventType::WindowResize; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "WindowResize"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };

	private:
		float width, height;
	};

	class WindowCloseEvent : public Event {
	public:
		WindowCloseEvent() = default;
		static EventType get_static_type() { return EventType::WindowClose; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "WindowClose"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };
	};

	class AppTickEvent : public Event {
	public:
		AppTickEvent() = default;
		static EventType get_static_type() { return EventType::AppTick; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "AppTick"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };
	};

	class AppRenderEvent : public Event {
	public:
		AppRenderEvent() = default;
		static EventType get_static_type() { return EventType::AppRender; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "AppRender"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };
	};

	class AppUpdateEvent : public Event {
	public:
		AppUpdateEvent() = default;
		static EventType get_static_type() { return EventType::AppUpdate; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "AppUpdate"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryApplication; };
	};

} // namespace ForgottenEngine
