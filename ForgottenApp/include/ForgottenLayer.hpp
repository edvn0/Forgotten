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

private:
	void find_entity();
	static void ui_toolbar();

	void load_model(const std::filesystem::path& path);

	void on_overlay_render();

private:
	glm::vec2 viewport_size = { 0.0f, 0.0f };
	glm::vec2 viewport_bounds[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
	bool viewport_focused = false, viewport_hovered = false;

	int gizmo_type = -1;
};
