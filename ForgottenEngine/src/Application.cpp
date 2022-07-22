#include "fg_pch.hpp"

#include "Application.hpp"
#include "Clock.hpp"
#include "Input.hpp"

namespace ForgottenEngine {

Application* Application::instance = nullptr;

Application::Application(const ApplicationProperties& props)
{
	if (instance) {
		CORE_ERROR("Application already exists.");
	}
	instance = this;

	window = std::unique_ptr<Window>(Window::create(props));
	window->init();
	window->set_event_callback([&](Event& event) { this->on_event(event); });

	add_overlay(std::make_unique<ImGuiLayer>());
};

Application::~Application() = default;

void Application::run()
{
	on_init();
	auto nr_frames = 0;
	last_time = Clock::get_time<float>();
	while (is_running) {
		auto current_time = Clock::get_time<float>();
		nr_frames++;

		TimeStep time_step(current_time - last_frame_time);
		last_frame_time = current_time;

		auto step = current_time - last_time;

		if (step >= 1.0) {
			frame_time = 1000.0f / static_cast<float>(nr_frames);
			nr_frames = 0;
			last_time = Clock::get_time<float>();
		}

		{
			for (auto& l : stack) {
				l->on_update(time_step);
			}
		}
		{
			ImGuiLayer::begin();
			for (auto& l : stack) {
				l->on_ui_render(time_step);
			}
		}

		{
			window->on_update();
		}
		engine->update(time_step);
	}
}

void Application::on_event(Event& event)
{
	EventDispatcher dispatcher(event);

	dispatcher.dispatch_event<WindowCloseEvent>([&](WindowCloseEvent& e) {
		(void)e;

		is_running = false;
		return true;
	});

	dispatcher.dispatch_event<WindowResizeEvent>([&](WindowResizeEvent& e) {
		window->resize_window(e.get_width(), e.get_height());
		window->resize_framebuffer(e.get_width(), e.get_height());
		return false;
	});

	for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
		if (event) {
			break;
		}
		auto& layer = *it;
		layer->on_event(event);
	}
}

void Application::add_layer(std::unique_ptr<Layer> layer) { stack.push(std::move(layer)); }

void Application::add_overlay(std::unique_ptr<Layer> overlay) { stack.push_overlay(std::move(overlay)); }

}
