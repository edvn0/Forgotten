#pragma once

#include "Common.hpp"

#include "LayerStack.hpp"
#include "TimeStep.hpp"
#include "Window.hpp"

#include <queue>

#include "events/ApplicationEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"

#include "imgui/ImGuiLayer.hpp"

namespace ForgottenEngine {

class Application {
	using EventCallbackFn = std::function<void(Event&)>;

public:
	explicit Application(const ApplicationProperties& props);
	virtual ~Application();

	void run();

	virtual void on_init(){};
	void on_event(Event& event);
	void add_layer(std::unique_ptr<Layer> layer);
	void add_overlay(std::unique_ptr<Layer> overlay);

	template <typename Func> inline void queue_event(Func&& f) { event_queue.push(f); }

	template <typename TEvent, bool DispatchImmediately = false, typename... TEventArgs>
	inline void dispatch_event(TEventArgs&&... args)
	{
		static_assert(std::is_assignable_v<Event, TEvent>);

		std::shared_ptr<TEvent> event = std::make_shared<TEvent>(std::forward<TEventArgs>(args)...);
		if constexpr (DispatchImmediately) {
			on_event(*event);
		} else {
			std::scoped_lock<std::mutex> lock(event_queue_mutex);
			event_queue.push([event]() { Application::the().on_event(*event); });
		}
	}

	static inline Application& the() { return *instance; }
	Window& get_window();
	inline const auto& get_imgui_layer() { return stack.get_imgui_layer(); }
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

	TimeStep time_step{ 0.0f };
	TimeStep frame_time{ 0.0f };
	float last_frame_time{ 0.0f };

private:
	static Application* instance;

	bool is_minimized{ false };

	std::mutex event_queue_mutex;
	std::queue<std::function<void()>> event_queue;
	std::vector<EventCallbackFn> event_callbacks;

	ImGuiLayer* imgui_layer() { return dynamic_cast<ImGuiLayer*>(get_imgui_layer().get()); }

	void render_imgui(TimeStep step);
	void process_events();
};

Application* create_application(const ApplicationProperties& props);
}
