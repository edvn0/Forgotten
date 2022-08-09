#pragma once

#include "fg.hpp"

using namespace ForgottenEngine;

class ForgottenLayer : public Layer {
public:
	ForgottenLayer();
	~ForgottenLayer() override = default;

	void on_attach() override;
	void on_event(Event& e) override;
	void on_update(const TimeStep& ts) override;
	void on_ui_render(const TimeStep& ts) override;
	void on_detach() override;

	void draw_debug_stats();
	void draw_string(const std::string& string, const glm::vec2& position,
		const glm::vec4& color = glm::vec4(1.0f), float size = 50.0f);

	void update_fps_stats();
	void update_performance_timers();

private:
	static void ui_toolbar();

	void on_overlay_render();

private:
	glm::vec2 viewport_size = { 0.0f, 0.0f };
	glm::vec2 viewport_bounds[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
	bool viewport_focused = false, viewport_hovered = false;

	Reference<Renderer2D> renderer;

	glm::mat4 projection_matrix{ 1.0f };
	Reference<Pipeline> swapchain_pipeline;
	Reference<Material> swapchain_material;
	Reference<RenderCommandBuffer> command_buffer;
	float m_Width{ 1280 };
	float m_Height{ 720 };
	TimeStep frame_time{ 0.0f };
	TimeStep update_fps_timer{ 0.0f };
	TimeStep update_performance_timer{ 0.0f };
	TimeStep frames_per_second{ 0.0f };
};
