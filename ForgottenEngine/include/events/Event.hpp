#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

	enum class EventType {
		None = 0,
		WindowClose,
		WindowResize,
		WindowFocus,
		WindowLostFocus,
		WindowMoved,
		WindowFramebuffer,
		AppTick,
		AppUpdate,
		AppRender,
		KeyPressed,
		KeyReleased,
		KeyTyped,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseMoved,
		MouseScrolled
	};

	enum EventCategory {
		None = 0,
		EventCategoryApplication = BIT(0),
		EventCategoryInput = BIT(1),
		EventCategoryKeyboard = BIT(2),
		EventCategoryMouse = BIT(3),
		EventCategoryMouseButton = BIT(4),
	};

	class Event {
		friend class EventDispatcher;

	public:
		[[nodiscard]] virtual EventType get_event_type() const = 0;
		[[nodiscard]] virtual const char* get_name() const = 0;
		[[nodiscard]] virtual int get_category() const = 0;
		[[nodiscard]] virtual std::string to_string() const { return get_name(); }

		[[nodiscard]] inline bool is_in_category(EventCategory category) const { return get_category() & category; }

		explicit operator bool() const { return handled; }

		bool& get_handled() { return handled; }

		bool handled = false;
	};

	class EventDispatcher {
		template <typename T> using EventFn = std::function<bool(T&)>;

	public:
		explicit EventDispatcher(Event& event)
			: event(event) {};

		template <typename T = Event> bool dispatch_event(EventFn<T> func)
		{
			if (event.get_event_type() == T::get_static_type()) {
				event.handled = func(*(T*)&event);
				return true;
			}
			return false;
		}

	private:
		Event& event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		os << e.to_string();
		return os;
	}

} // namespace ForgottenEngine
