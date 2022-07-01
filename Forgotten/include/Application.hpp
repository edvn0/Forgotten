#pragma once

#include "Layer.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <vulkan/vulkan.h>

struct GLFWwindow;
struct ImGui_ImplVulkanH_Window;
void check_vk_result(VkResult err);

namespace Forgotten {

struct ApplicationProperties {
	std::string name = "ForgottenApp";
	size_t width = 1600;
	size_t height = 900;
};

class Application {
public:
	Application(ApplicationProperties props = { .name = "ForgottenApp", .width = 1600, .height = 900 });
	~Application();

	void construct_and_run();
	void close();

	void set_menubar_callback(const std::function<void()>& cb) { menubar_callback = cb; }

	template <typename T = Layer> void push_layer()
	{
		static_assert(std::is_base_of<Layer, T>::value, "Added layer does not subclass Layer.");
		layer_stack.emplace_back(std::make_shared<T>())->on_attach();
	}

	void push_layer(const std::shared_ptr<Layer>& layer)
	{
		layer_stack.emplace_back(layer);
		layer->on_attach();
	}

	static VkCommandBuffer get_command_buffer();
	static void flush_command_buffer(VkCommandBuffer command_buffer);

	static void submit_resource_free(std::function<void()>&& free_func);

	static float get_frame_time();
	static float get_average_fps();

private:
	void construct();
	void destruct();

	static void render_and_present(ImGuiIO& io, ImGui_ImplVulkanH_Window* window_data, const ImVec4& cc);
	static void resize_swap_chain(GLFWwindow* wh);

private:
	ApplicationProperties spec;
	GLFWwindow* window_handle = nullptr;
	bool running = false;

	std::function<void()> menubar_callback;
	std::vector<std::shared_ptr<Layer>> layer_stack;
};

Application* create_application(int argc, char** argv);

} // namespace Forgotten
