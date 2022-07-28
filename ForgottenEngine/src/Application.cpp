#include "fg_pch.hpp"

#include "Application.hpp"
#include "Clock.hpp"
#include "Input.hpp"

#include "render/Renderer.hpp"

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

	Renderer::init();
	Renderer::wait_and_render();
	add_overlay(std::make_unique<ImGuiLayer>());
};

Application::~Application()
{
	Renderer::wait_and_render();
	Renderer::shut_down();
};

void Application::run()
{
	on_init();
	while (is_running) {
		static uint64_t frame_counter = 0;
		process_events();

		if (!is_minimized) {
			Renderer::begin_frame();
			{
				for (const auto& layer : stack)
					layer->on_update(time_step);
			}

			// Render ImGui on render thread
			Application* app = this;
			{
				Renderer::submit([app, ts = time_step]() { app->render_imgui(ts); });
				Renderer::submit([app]() { app->imgui_layer()->end(); });
			}
			Renderer::end_frame();

			window->get_swapchain().begin_frame();
			Renderer::wait_and_render();
			window->swap_buffers();
		}

		auto time = Clock::get_time<float>();
		frame_time = TimeStep(time - last_frame_time);
		time_step = TimeStep(glm::min<float>(frame_time, 0.0333f));
		last_frame_time = time;

		// HZ_CORE_INFO("-- END FRAME {0}", frame_counter);
		frame_counter++;
	}
}

void Application::on_event(Event& event)
{
	EventDispatcher dispatcher(event);

	dispatcher.dispatch_event<WindowCloseEvent>([&](WindowCloseEvent& e) {
		is_running = false;
		return true;
	});

	dispatcher.dispatch_event<WindowResizeEvent>([&](WindowResizeEvent& e) {
		if (e.get_width() == 0 || e.get_height() == 0) {
			return false;
		}

		window->get_swapchain().on_resize(
			static_cast<uint32_t>(e.get_width()), static_cast<uint32_t>(e.get_height()));
		return false;
	});

	if (event.handled)
		return;

	for (auto& event_cb : event_callbacks) {
		event_cb(event);

		if (event.handled)
			break;
	}

	for (auto it = stack.rbegin(); it != stack.rend(); ++it) { // NOLINT(modernize-loop-convert)
		if (event) {
			break;
		}
		auto& layer = *it;
		layer->on_event(event);
	}
}

void Application::add_layer(std::unique_ptr<Layer> layer) { stack.push(std::move(layer)); }

void Application::add_overlay(std::unique_ptr<Layer> overlay) { stack.push_overlay(std::move(overlay)); }

Window& Application::get_window() { return *window; }

void Application::render_imgui(TimeStep step)
{
	imgui_layer()->begin();

	for (auto& l : stack)
		l->on_ui_render(step);
}

void Application::process_events() { window->process_events(); }

}
