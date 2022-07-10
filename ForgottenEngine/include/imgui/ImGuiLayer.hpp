//
// Created by Edwin Carlsson on 2022-07-09.
//

#pragma once

#include "Layer.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

namespace ForgottenEngine {

class ImGuiLayer : public Layer {
private:
	bool block{ false };

public:
	ImGuiLayer()
		: Layer("ImGuiLayer"){};

	~ImGuiLayer() override = default;

	static void begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	static void end(VkCommandBuffer cmd)
	{
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	void on_update(const TimeStep& step) override;
	void on_event(Event& e) override;
	void on_detach() override;

	void should_block(bool should_block) { block = should_block; }
};

}