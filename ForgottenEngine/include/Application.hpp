#pragma once

#include "Common.hpp"
#include "events/ApplicationEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"
#include "imgui/ImGuiLayer.hpp"
#include "LayerStack.hpp"
#include "TimeStep.hpp"
#include "Window.hpp"

#include <concepts>
#include <queue>
#include <type_traits>

namespace ForgottenEngine {

	class Application {
		using EventCallbackFn = std::function<void(Event&)>;

	public:
		explicit Application(const ApplicationProperties& props);
		virtual ~Application();

		void run();

		virtual void on_init() {};
		void on_event(Event& event);
		void add_layer(std::unique_ptr<Layer> layer);
		void add_overlay(std::unique_ptr<Layer> overlay);

		template <typename Func> inline void queue_event(Func&& f) { event_queue.push(f); }

		template <typename TEvent, bool DispatchImmediately = false, typename... TEventArgs>
		requires std::assignable_from<Event, TEvent>
		inline void dispatch_event(TEventArgs&&... args)
		{
			std::unique_ptr<TEvent> event = std::make_unique<TEvent>(std::forward<TEventArgs>(args)...);
			if constexpr (DispatchImmediately) {
				on_event(*event);
			} else {
				std::scoped_lock<std::mutex> lock(event_queue_mutex);
				event_queue.push([event]() { Application::the().on_event(*event); });
			}
		}

		static inline Application& the() { return *instance; }
		static std::string_view platform_name();
		Window& get_window();
		inline Layer* get_imgui_layer() { return stack.get_imgui_layer(); }
		[[nodiscard]] inline float get_frametime() const { return frame_time; };

		inline bool exit()
		{
			is_running = false;
			return true;
		}

	protected:
		std::unique_ptr<Window> window;

		bool is_running = true;
		LayerStack stack;

		TimeStep time_step { 0.0f };
		TimeStep frame_time { 0.0f };
		float last_frame_time { 0.0f };

	private:
		static Application* instance;

		bool is_minimized { false };

		std::mutex event_queue_mutex;
		std::queue<std::function<void()>> event_queue;
		std::vector<EventCallbackFn> event_callbacks;

		ImGuiLayer* imgui_layer() { return dynamic_cast<ImGuiLayer*>(get_imgui_layer()); }

		void render_imgui(TimeStep step);
		void process_events();
	};

	Application* create_application(const ApplicationProperties& props);
} // namespace ForgottenEngine
