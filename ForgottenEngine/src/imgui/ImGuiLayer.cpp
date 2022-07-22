//
// Created by Edwin Carlsson on 2022-07-09.
//

#include "fg_pch.hpp"

#include "imgui/ImGuiLayer.hpp"

#include "Application.hpp"
#include "Input.hpp"

#include "GLFW/glfw3.h"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

void ImGuiLayer::on_attach()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
}

void ImGuiLayer::on_detach()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void ImGuiLayer::on_update(const TimeStep& step)
{
	if (Input::key(Key::Escape)) {
		Application::the().exit();
	}
}

void ImGuiLayer::on_event(Event& e)
{
	if (block) {
		auto& io = ImGui::GetIO();
		e.handled |= e.is_in_category(EventCategoryMouse) & io.WantCaptureMouse;
		e.handled |= e.is_in_category(EventCategoryKeyboard) & io.WantCaptureKeyboard;
	}
}

}