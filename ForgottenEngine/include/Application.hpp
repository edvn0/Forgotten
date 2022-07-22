#pragma once

#include "Common.hpp"

#include "LayerStack.hpp"
#include "TimeStep.hpp"
#include "Window.hpp"

#include "render/Renderer.hpp"

#include "events/ApplicationEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"

#include "imgui/ImGuiLayer.hpp"
#include "vulkan/VulkanEngine.hpp"

namespace ForgottenEngine {

class Application {
public:
	static constexpr double FRAME_UPDATE_INTERVAL_S = 1.0;

public:
	explicit Application(const ApplicationProperties& props);
	virtual ~Application();

	void run();
	virtual void cleanup()
	{
		engine->cleanup();
		Renderer::shut_down();
	}

	virtual void on_init(){};
	void on_event(Event& event);
	void add_layer(std::unique_ptr<Layer> layer);
	void add_overlay(std::unique_ptr<Layer> overlay);

	static inline Application& the() { return *instance; }
	inline Window& get_window() { return *window; }
	inline const std::unique_ptr<Layer>& get_imgui_layer() { return stack.get_imgui_layer(); }
	[[nodiscard]] inline float get_frametime() const { return frame_time; };
	inline void set_engine(std::unique_ptr<VulkanEngine>&& created) { engine = std::move(created); }

	inline bool exit()
	{
		is_running = false;
		return true;
	}

protected:
	std::unique_ptr<Window> window;
	std::unique_ptr<VulkanEngine> engine;

	bool is_running = true;
	LayerStack stack;

	TimeStep::TimeDelta last_time = 0.0f;
	float frame_time = 0.0f;
	float last_frame_time = 0.0f;

private:
	static Application* instance;
};

Application* create_application(const ApplicationProperties& props);
}