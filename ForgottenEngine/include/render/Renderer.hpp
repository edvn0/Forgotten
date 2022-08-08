#pragma once

#include "ApplicationProperties.hpp"
#include "Reference.hpp"
#include "render/RenderCommandQueue.hpp"
#include "render/Shader.hpp"
#include "vulkan/VulkanSwapchain.hpp"

namespace ForgottenEngine {

class RendererContext;
class ShaderLibrary;
class Pipeline;
class UniformBufferSet;
class StorageBufferSet;
class Material;
class VertexBuffer;
class IndexBuffer;
class Texture2D;
class RenderPass;
class Shader;

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

	static void begin_render_pass(const Reference<RenderCommandBuffer>& command_buffer,
		Reference<RenderPass> render_pass, bool explicit_clean);
	static void end_render_pass(const Reference<RenderCommandBuffer>& command_buffer);
	static void end_frame();

	// Submits
	static void render_geometry(const Reference<RenderCommandBuffer>&, const Reference<Pipeline>&,
		const Reference<UniformBufferSet>&, const Reference<StorageBufferSet>&, const Reference<Material>&,
		const Reference<VertexBuffer>&, const Reference<IndexBuffer>&, const glm::mat4& transform,
		uint32_t index_count);
	// end submits

	// Registrations
	static void register_shader_dependency(const Reference<Shader>& shader, Reference<Pipeline> pipeline);
	static void register_shader_dependency(const Reference<Shader>& shader, Reference<Material> material);

	// end Registrations

	// shaders and macros
	static void on_shader_reloaded(size_t hash);

	static const std::unordered_map<std::string, std::string>& get_global_shader_macros();

	static void acknowledge_parsed_global_macros(
		const std::unordered_set<std::string>& macros, Reference<Shader> shader);

	static void set_macro_in_shader(
		Reference<Shader> shader, const std::string& name, const std::string& value = "");

	static void set_global_macro_in_shaders(const std::string& name, const std::string& value = "");

	static bool update_dirty_shaders();
	// end shaders and macros

	static Reference<Texture2D> get_white_texture();

	static RendererConfig& get_config();

	static Reference<ShaderLibrary> get_shader_library();

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
			const uint32_t index = Renderer::get_current_frame_index();
			auto storage_buffer = get_render_resource_free_queue(index).allocate(render_command, sizeof(func));
			new (storage_buffer) SubmittedFunction(std::forward<SubmittedFunction>((SubmittedFunction &&) func));
		});
	}

	static RenderCommandQueue& get_render_resource_free_queue(uint32_t index);
	static uint32_t get_current_frame_index();

private:
	static RenderCommandQueue& get_render_command_queue();
};

}
