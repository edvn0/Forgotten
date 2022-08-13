#pragma once

#include "codes/MouseCode.hpp"
#include "events/Event.hpp"

namespace ForgottenEngine {

	class MouseMovedEvent : public Event {
	public:
		MouseMovedEvent(float x, float y)
			: mouse_x(x)
			, mouse_y(y)
		{
		}

		[[nodiscard]] inline float get_x() const { return mouse_x; }
		[[nodiscard]] inline float get_y() const { return mouse_y; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "MouseMovedEvent: " << mouse_x << ", " << mouse_y;
			return ss.str();
		}

		static EventType get_static_type() { return EventType::MouseMoved; };
		[[nodiscard]] [[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] [[nodiscard]] const char* get_name() const override { return "MouseMoved"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryMouse | EventCategory::EventCategoryInput; }

	private:
		float mouse_x, mouse_y;
	};

	class MouseScrolledEvent : public Event {
	public:
		MouseScrolledEvent(float x, float y)
			: offset_x(x)
			, offset_y(y)
		{
		}

		[[nodiscard]] inline float get_offset_x() const { return offset_x; }
		[[nodiscard]] inline float get_offset_y() const { return offset_y; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << offset_x << ", " << offset_y;
			return ss.str();
		}

		static EventType get_static_type() { return EventType::MouseScrolled; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "MouseScrolled"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryMouse | EventCategory::EventCategoryInput; }

	private:
		float offset_x, offset_y;
	};

	class MouseButtonPressedEvent : public Event {
	public:
		MouseButtonPressedEvent(MouseCode mouse_button_, float x_, float y_)
			: mouse_button(mouse_button_)
			, x(x_)
			, y(y_) {};

		[[nodiscard]] inline MouseCode get_mouse_button() const { return mouse_button; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << (int)mouse_button;
			return ss.str();
		}

		[[nodiscard]] inline float get_x() const { return x; };
		[[nodiscard]] inline float get_y() const { return y; };

		static EventType get_static_type() { return EventType::MouseButtonPressed; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "MouseButtonPressed"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryMouseButton | EventCategory::EventCategoryInput; }

	private:
		MouseCode mouse_button;
		float x, y;
	};

	class MouseButtonReleasedEvent : public Event {
	public:
		MouseButtonReleasedEvent(MouseCode mouse_button_, float x_, float y_)
			: mouse_button(mouse_button_)
			, x(x_)
			, y(y_) {};

		[[nodiscard]] inline MouseCode get_mouse_button() const { return mouse_button; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << (int)mouse_button;
			return ss.str();
		}

		[[nodiscard]] inline float get_x() const { return x; };
		[[nodiscard]] inline float get_y() const { return y; };

		static EventType get_static_type() { return EventType::MouseButtonReleased; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "MouseButtonReleased"; };
		[[nodiscard]] int get_category() const override { return EventCategory::EventCategoryMouseButton | EventCategory::EventCategoryInput; }

	private:
		MouseCode mouse_button;
		float x {}, y {};
	};
} // namespace ForgottenEngine
