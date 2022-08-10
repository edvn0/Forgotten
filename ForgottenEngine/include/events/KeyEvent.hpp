#pragma once

#include "codes/KeyCode.hpp"
#include "events/Event.hpp"

namespace ForgottenEngine {

	class KeyEvent : public Event {
	protected:
		explicit KeyEvent(KeyCode keycode)
			: key_code(keycode)
		{
		}

	public:
		[[nodiscard]] inline KeyCode get_key_code() const { return key_code; }

		[[nodiscard]] int get_category() const override
		{
			return EventCategory::EventCategoryKeyboard | EventCategory::EventCategoryInput;
		};

	protected:
		KeyCode key_code;
	};

	class KeyPressedEvent : public KeyEvent {
	public:
		explicit KeyPressedEvent(KeyCode keycode, int repeats = 0)
			: KeyEvent(keycode)
			, repeat_count(repeats) {};

		[[nodiscard]] inline int get_repeat_count() const { return repeat_count; }

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << (int)key_code << ", " << repeat_count;
			return ss.str();
		}

		static EventType get_static_type() { return EventType::KeyPressed; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "KeyPressed"; };

	private:
		int repeat_count;
	};

	class KeyReleasedEvent : public KeyEvent {
	public:
		explicit KeyReleasedEvent(KeyCode keycode)
			: KeyEvent(keycode) {};

		static EventType get_static_type() { return EventType::KeyReleased; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "KeyReleased"; };
	};

	class KeyTypedEvent : public KeyEvent {
	public:
		explicit KeyTypedEvent(KeyCode keycode)
			: KeyEvent(keycode) {};

		[[nodiscard]] std::string to_string() const override
		{
			std::stringstream ss;
			ss << "KeyTypedEvent: " << (int)key_code;
			return ss.str();
		}

		static EventType get_static_type() { return EventType::KeyTyped; };
		[[nodiscard]] EventType get_event_type() const override { return get_static_type(); };
		[[nodiscard]] const char* get_name() const override { return "KeyTyped"; };
	};
} // namespace ForgottenEngine