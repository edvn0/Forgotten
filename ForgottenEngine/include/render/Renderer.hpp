#pragma once

#include "Reference.hpp"
#include "render/RenderCommandQueue.hpp"
#include "vulkan/VulkanSwapchain.hpp"

namespace ForgottenEngine {

class RendererConfig;
class RendererContext;

class Renderer {
public:
	Renderer() = delete;
	Renderer(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;

public:
	static Reference<RendererContext> get_context();

	static void init();
	static void shut_down();

	static void wait_and_render();

	static void begin_frame();
	static void end_frame();

	static RendererConfig& get_config();

public:
	template <typename SubmittedFunction> static void submit(SubmittedFunction&& func)
	{
		auto render_command = [](void* ptr) {
			auto function_pointer = (SubmittedFunction*)ptr;
			(*function_pointer)();

			// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
			// however some items like uniforms which contain std::strings still exist for now
			// static_assert(std::is_trivially_destructible_v<SubmittedFunction>, "SubmittedFunction must be
			// trivially destructible");
			function_pointer->~SubmittedFunction();
		};
		auto storage_buffer = get_render_command_queue().allocate(render_command, sizeof(func));
		new (storage_buffer) SubmittedFunction(std::forward<SubmittedFunction>(func));
	}

	template <typename SubmittedFunction> static void submit_resource_free(SubmittedFunction&& func)
	{
		auto render_command = [](void* ptr) {
			auto function_pointer = (SubmittedFunction*)ptr;
			(*function_pointer)();

			// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
			// however some items like uniforms which contain std::strings still exist for now
			// static_assert(std::is_trivially_destructible_v<SubmittedFunction>, "SubmittedFunction must be
			// trivially destructible");
			function_pointer->~SubmittedFunction();
		};

		Renderer::submit([render_command, func]() {
			const uint32_t index = 0; // Renderer::GetCurrentFrameIndex();
			auto storage_buffer = get_render_resource_free_queue(index).allocate(render_command, sizeof(func));
			new (storage_buffer) SubmittedFunction(std::forward<SubmittedFunction>((SubmittedFunction &&) func));
		});
	}

	static RenderCommandQueue& get_render_resource_free_queue(uint32_t index);

private:
	static RenderCommandQueue& get_render_command_queue();
};

}
