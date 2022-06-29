#pragma once

#include "Layer.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <vulkan/vulkan.h>

struct GLFWwindow;
void check_vk_result(VkResult err);

namespace Forgotten {

struct ApplicationProperties {
	std::string name = "ForgottenApp";
	size_t width = 1600;
	size_t height = 900;
};

class Application {
public:
	Application(const ApplicationProperties& props = { .name = "ForgottenApp", .width = 1600, .height = 900 });
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

	static VkInstance get_instance();
	static VkPhysicalDevice get_physical_device();
	static VkDevice get_device();

	static VkCommandBuffer get_command_buffer(bool begin);
	static void flush_command_buffer(VkCommandBuffer command_buffer);

	static void submit_resource_free(std::function<void()>&& free_func);

private:
	void construct();
	void destruct();

private:
	ApplicationProperties spec;
	GLFWwindow* window_handle = nullptr;
	bool running = false;

	std::function<void()> menubar_callback;
	std::vector<std::shared_ptr<Layer>> layer_stack;
};

Application* create_application(int argc, char** argv);

} // namespace Forgotten
