//
// Created by Edwin Carlsson on 2022-07-09.
//

#pragma once

#include "Layer.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

typedef VkCommandPool_T* VkCommandPool;

namespace ForgottenEngine {

	class ImGuiLayer : public Layer {
	private:
		bool block { false };

	public:
		ImGuiLayer()
			: Layer("ImGui") {};

		~ImGuiLayer() override = default;

		void begin()
		{
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
		}

		void end();

		void on_attach() override;
		void on_update(const TimeStep& step) override;
		void on_event(Event& e) override;
		void on_detach() override;

		void should_block(bool should_block) { block = should_block; }
	};

} // namespace ForgottenEngine