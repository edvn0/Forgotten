#pragma once

#include "render/RenderCommandQueue.hpp"

namespace ForgottenEngine {

class RenderCommandQueue;

class Renderer {
public:
	Renderer() = delete;
	Renderer(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;

public:
	static void init();
	static void shut_down();

	static void wait_and_render();

	static void begin_frame();
	static void end_frame();

public:
	template <typename FuncT> static void submit(FuncT&& func)
	{
		auto renderCmd = [](void* ptr) {
			auto pFunc = (FuncT*)ptr;
			(*pFunc)();

			// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
			// however some items like uniforms which contain std::strings still exist for now
			// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
			pFunc->~FuncT();
		};
		auto storageBuffer = get_render_command_queue().allocate(renderCmd, sizeof(func));
		new (storageBuffer) FuncT(std::forward<FuncT>(func));
	}

	template <typename FuncT> static void submit_resource_free(FuncT&& func)
	{
		auto renderCmd = [](void* ptr) {
			auto pFunc = (FuncT*)ptr;
			(*pFunc)();

			// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
			// however some items like uniforms which contain std::strings still exist for now
			// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
			pFunc->~FuncT();
		};

		Submit([renderCmd, func]() {
			const uint32_t index = 0; // Renderer::GetCurrentFrameIndex();
			auto storageBuffer = Renderer::get_render_resource_free_queue(index).allocate(renderCmd, sizeof(func));
			new (storageBuffer) FuncT(std::forward<FuncT>((FuncT &&) func));
		});
	}

	RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index);

private:
	static RenderCommandQueue& get_render_command_queue();
};

}