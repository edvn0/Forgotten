#include "fg_pch.hpp"

#include "render/Renderer.hpp"

#include "Application.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderCommandQueue.hpp"
#include "render/RenderPass.hpp"
#include "render/RendererAPI.hpp"
#include "render/Shader.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/VulkanSwapchain.hpp"

namespace std {
	template <>
	struct hash<ForgottenEngine::WeakReference<ForgottenEngine::Shader>> {
		size_t operator()(const ForgottenEngine::WeakReference<ForgottenEngine::Shader>& shader) const noexcept
		{
			return shader->get_hash();
		}
	};
} // namespace std

namespace ForgottenEngine {

	struct GlobalShaderInfo {
		std::unordered_map<std::string, std::unordered_map<size_t, WeakReference<Shader>>> shader_global_macros_map;
		std::unordered_set<WeakReference<Shader>> dirty_shaders;
	};
	static GlobalShaderInfo global_shaders;

	struct RendererData {
	public:
		size_t* temp { nullptr };
		Reference<ShaderLibrary> shader_library;
		Reference<Texture2D> white_texture;
		std::unordered_map<std::string, std::string> global_shader_macros;
	};

	static RendererData* renderer_data;

	static RendererAPI* renderer_api = nullptr;
	static RenderCommandQueue resource_free_queue[3];
	static RenderCommandQueue* command_queue = nullptr;
	static RendererConfig config;

	static RendererAPI* init_renderer_api()
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::Vulkan:
			return new VulkanRenderer();
		default:
			CORE_ASSERT(false, "Unknown RendererAPI");
		}
	}

	struct ShaderDependencies {
		std::vector<Reference<Pipeline>> pipelines;
		std::vector<Reference<Material>> materials;
	};
	static std::unordered_map<size_t, ShaderDependencies> shader_dependencies;

	static inline auto& get() { return *renderer_data; }

	void Renderer::init()
	{
		CORE_DEBUG("Initializing renderer.");
		renderer_data = new RendererData();

		command_queue = new RenderCommandQueue();

		CORE_DEBUG("Initializing renderer API.");
		renderer_api = init_renderer_api();

		config.frames_in_flight = glm::min<uint32_t>(
			config.frames_in_flight, Application::the().get_window().get_swapchain().get_image_count());
		// Much stuff

		renderer_data->shader_library = Reference<ShaderLibrary>::create();

		//	if (!config.shader_pack_path.empty())
		//		Renderer::get_shader_library()->load_shader_pack(config.shader_pack_path);

		// Load 2D Renderer Shaders.
		Renderer::get_shader_library()->load("Renderer2D.glsl");
		Renderer::get_shader_library()->load("Renderer2D_Circle.glsl");
		Renderer::get_shader_library()->load("Renderer2D_Line.glsl");
		Renderer::get_shader_library()->load("Renderer2D_Text.glsl");
		Renderer::get_shader_library()->load("TexturePass.glsl");

		ShaderPack::create_from_library(Renderer::get_shader_library(), "shader_pack.fgsp");

		Renderer::wait_and_render();

		renderer_api->init();
	}

	void Renderer::shut_down()
	{
		renderer_api->shut_down();
		delete renderer_data;

		shader_dependencies.clear();

		// Resource release queue
		for (uint32_t i = 0; i < config.frames_in_flight; i++) {
			auto& queue = Renderer::get_render_resource_free_queue(i);
			queue.execute();
		}

		delete command_queue;
	}

	void Renderer::wait_and_render() { command_queue->execute(); }

	void Renderer::begin_frame() { renderer_api->begin_frame(); }

	void Renderer::begin_render_pass(
		const Reference<RenderCommandBuffer>& command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
	{
		CORE_ASSERT(render_pass, "Render pass cannot be null!");

		renderer_api->begin_render_pass(command_buffer, render_pass, explicit_clear);
	}

	void Renderer::end_render_pass(const Reference<RenderCommandBuffer>& command_buffer)
	{
		renderer_api->end_render_pass(command_buffer);
	}

	void Renderer::end_frame() { renderer_api->end_frame(); }

	// Submits
	void Renderer::render_geometry(const Reference<RenderCommandBuffer>& cmd_buffer,
		const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& ubs,
		const Reference<StorageBufferSet>& sbs, const Reference<Material>& material, const Reference<VertexBuffer>& vb,
		const Reference<IndexBuffer>& ib, const glm::mat4& transform, uint32_t index_count)
	{
		return renderer_api->render_geometry(cmd_buffer, pipeline, ubs, sbs, material, vb, ib, transform, index_count);
	}

	void Renderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
		const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& uniformBufferSet,
		const Reference<Material>& material)
	{
		return renderer_api->submit_fullscreen_quad(command_buffer, pipeline, uniformBufferSet, material);
	}

	void Renderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
		const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& uniformBufferSet,
		const Reference<StorageBufferSet>& sb, const Reference<Material>& material)
	{
		return renderer_api->submit_fullscreen_quad(command_buffer, pipeline, uniformBufferSet, sb, material);
	}
	// End submits

	// Start Registrations and shaders
	void Renderer::register_shader_dependency(const Reference<Shader>& shader, Reference<Pipeline> pipeline)
	{
		shader_dependencies[shader->get_hash()].pipelines.push_back(pipeline);
	}

	void Renderer::register_shader_dependency(const Reference<Shader>& shader, Reference<Material> material)
	{
		shader_dependencies[shader->get_hash()].materials.push_back(material);
	}

	void Renderer::on_shader_reloaded(size_t hash)
	{
		if (shader_dependencies.find(hash) != shader_dependencies.end()) {
			auto& dependencies = shader_dependencies.at(hash);
			for (auto& pipeline : dependencies.pipelines) {
				pipeline->invalidate();
			}

			for (auto& material : dependencies.materials) {
				material->on_shader_reloaded();
			}
		}
	}

	void Renderer::acknowledge_parsed_global_macros(
		const std::unordered_set<std::string>& macros, Reference<Shader> shader)
	{
		for (const std::string& macro : macros) {
			global_shaders.shader_global_macros_map[macro][shader->get_hash()] = shader;
		}
	}

	void Renderer::set_macro_in_shader(Reference<Shader> shader, const std::string& name, const std::string& value)
	{
		shader->set_macro(name, value);
		global_shaders.dirty_shaders.emplace(shader.raw());
	}

	void Renderer::set_global_macro_in_shaders(const std::string& name, const std::string& value)
	{
		if (renderer_data->global_shader_macros.find(name) != renderer_data->global_shader_macros.end()) {
			if (renderer_data->global_shader_macros.at(name) == value)
				return;
		}

		renderer_data->global_shader_macros[name] = value;

		if (global_shaders.shader_global_macros_map.find(name) == global_shaders.shader_global_macros_map.end()) {
			CORE_WARN("No shaders with {} macro found", name);
			return;
		}

		CORE_ASSERT(
			global_shaders.shader_global_macros_map.find(name) != global_shaders.shader_global_macros_map.end(),
			"Macro has not been passed from any shader!");
		for (auto& [hash, shader] : global_shaders.shader_global_macros_map.at(name)) {
			CORE_ASSERT(shader.is_valid(), "Shader is deleted!");
			global_shaders.dirty_shaders.emplace(shader);
		}
	}

	bool Renderer::update_dirty_shaders()
	{
		const bool updated_any_shader = global_shaders.dirty_shaders.size();
		for (auto shader : global_shaders.dirty_shaders) {
			CORE_ASSERT(shader.is_valid(), "Shader is deleted!");
			shader->rt_reload(true);
		}
		global_shaders.dirty_shaders.clear();

		return updated_any_shader;
	}

	const std::unordered_map<std::string, std::string>& Renderer::get_global_shader_macros()
	{
		return renderer_data->global_shader_macros;
	}
	// end Registrations

	RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index) { return resource_free_queue[index]; }

	RenderCommandQueue& Renderer::get_render_command_queue() { return *command_queue; }

	RendererConfig& Renderer::get_config() { return config; }

	Reference<RendererContext> Renderer::get_context() { return Application::the().get_window().get_context(); }

	Reference<ShaderLibrary> Renderer::get_shader_library() { return renderer_data->shader_library; }

	uint32_t Renderer::get_current_frame_index()
	{
		return Application::the().get_window().get_swapchain().get_current_buffer_index();
	}

	Reference<Texture2D> Renderer::get_white_texture() { return renderer_data->white_texture; }

} // namespace ForgottenEngine
