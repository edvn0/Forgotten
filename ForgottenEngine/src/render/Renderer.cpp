#include "fg_pch.hpp"

#include "render/Renderer.hpp"

#include "Application.hpp"
#include "render/ComputePipeline.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderCommandQueue.hpp"
#include "render/RendererAPI.hpp"
#include "render/RenderPass.hpp"
#include "render/SceneEnvironment.hpp"
#include "render/Shader.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/VulkanSwapchain.hpp"

namespace std {
	template <> struct hash<ForgottenEngine::WeakReference<ForgottenEngine::Shader>> {
		size_t operator()(const ForgottenEngine::WeakReference<ForgottenEngine::Shader>& shader) const noexcept { return shader->get_hash(); }
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
		Reference<ShaderLibrary> shader_library;
		Reference<Texture2D> white_texture;
		Reference<Texture2D> black_texture;
		Reference<Texture2D> brdf_lut;
		Reference<Texture2D> hilbert_lut;
		Reference<SceneEnvironment> empty_env;
		Reference<TextureCube> black_cube;
		std::unordered_map<std::string, std::string> global_shader_macros;
	};

	RendererData renderer_data;
	std::unique_ptr<RendererAPI> renderer_api;

	static RenderCommandQueue resource_free_queue[3];
	static RendererConfig config;

	static std::unique_ptr<RendererAPI> init_renderer_api()
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::Vulkan:
			return std::make_unique<VulkanRenderer>();
		default:
			core_assert(false, "Unknown RendererAPI");
		}
	}

	struct ShaderDependencies {
		std::vector<Reference<Pipeline>> pipelines;
		std::vector<Reference<Material>> materials;
		std::vector<Reference<ComputePipeline>> computations;
	};
	static std::unordered_map<size_t, ShaderDependencies> shader_dependencies;

	void Renderer::init()
	{
		CORE_DEBUG("Initializing renderer.");
		renderer_api = init_renderer_api();

		auto fif = Application::the().get_window().get_swapchain().get_image_count();
		config.frames_in_flight = glm::min<uint32_t>(config.frames_in_flight, fif);
		// Much stuff

		renderer_data.shader_library = Reference<ShaderLibrary>::create();

		auto& shader_library = Renderer::get_shader_library();

		if (!config.shader_pack_path.empty())
			shader_library->load_shader_pack(config.shader_pack_path);

		shader_library->load("SceneComposite.glsl");
		shader_library->load("Renderer2D_Circle.glsl");
		shader_library->load("Renderer2D_Render.glsl");
		shader_library->load("Renderer2D_Line.glsl");
		shader_library->load("Renderer2D.glsl");
		shader_library->load("Renderer2D_Text.glsl");
		shader_library->load("TexturePass.glsl");
		shader_library->load("PreDepth.glsl");
		shader_library->load("LightCulling.glsl");
		shader_library->load("DirShadowMap.glsl");

		// Compile shaders
		Renderer::compile_shaders();

		uint32_t white_data = 0xffffffff;
		renderer_data.white_texture = Texture2D::create(ImageFormat::RGBA, 1, 1, &white_data);

		constexpr uint32_t black_data = 0xff000000;
		renderer_data.black_texture = Texture2D::create(ImageFormat::RGBA, 1, 1, &black_data);

		{
			TextureProperties props;
			props.SamplerWrap = TextureWrap::Clamp;
			const auto brdf_lut = Assets::find_resources_by_path(Assets::slashed_string_to_filepath("renderer/brdf_lut.tga"));

			core_assert(brdf_lut, "Could not find a file under {}", (*brdf_lut).string());

			renderer_data.brdf_lut = Texture2D::create((*brdf_lut).string(), props);
		}
		constexpr uint32_t black_cube_data[6] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
		renderer_data.black_cube = TextureCube::create(ImageFormat::RGBA, 1, 1, &black_cube_data);
		renderer_data.empty_env = Reference<SceneEnvironment>::create(renderer_data.black_cube, renderer_data.black_texture);

		// Hilbert look-up texture! It's a 64 x 64 uint16 texture
		{
			TextureProperties props;
			props.SamplerWrap = TextureWrap::Clamp;
			props.SamplerFilter = TextureFilter::Nearest;

			constexpr auto hilbert_index = [](uint32_t posX, uint32_t posY) {
				uint16_t index = 0u;
				for (uint16_t cur_level = 64 / 2u; cur_level > 0u; cur_level /= 2u) {
					const uint16_t region_x = (posX & cur_level) > 0u;
					const uint16_t region_y = (posY & cur_level) > 0u;
					index += cur_level * cur_level * ((3u * region_x) ^ region_y);
					if (region_y == 0u) {
						if (region_x == 1u) {
							posX = uint16_t((64 - 1u)) - posX;
							posY = uint16_t((64 - 1u)) - posY;
						}

						std::swap(posX, posY);
					}
				}
				return index;
			};

			uint16_t* data = new uint16_t[(size_t)(64 * 64)];
			for (int x = 0; x < 64; x++) {
				for (int y = 0; y < 64; y++) {
					const uint16_t r2index = hilbert_index(x, y);
					data[x + 64 * y] = r2index;
				}
			}
			renderer_data.hilbert_lut = Texture2D::create(ImageFormat::RED16UI, 64, 64, data, props);
			delete[] data;
		}

		if (!config.shader_pack_path.empty())
			ShaderPack::create_from_library(Renderer::get_shader_library(), "shader_pack.fgsp");

		Renderer::wait_and_render();

		renderer_api->init();
	}

	void Renderer::shut_down()
	{
		renderer_api->shut_down();

		shader_dependencies.clear();

		// Resource release queue
		for (uint32_t i = 0; i < config.frames_in_flight; i++) {
			auto& queue = Renderer::get_render_resource_free_queue(i);
			queue.execute();
		}
	}

	void Renderer::wait_and_render() { command_queue().execute(); }

	void Renderer::begin_frame() { renderer_api->begin_frame(); }

	void Renderer::begin_render_pass(const Reference<RenderCommandBuffer>& command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
	{
		core_assert(render_pass, "Render pass cannot be null!");

		renderer_api->begin_render_pass(command_buffer, render_pass, explicit_clear);
	}

	void Renderer::end_render_pass(const Reference<RenderCommandBuffer>& command_buffer) { renderer_api->end_render_pass(command_buffer); }

	void Renderer::end_frame() { renderer_api->end_frame(); }

	// Submits
	void Renderer::render_geometry(const Reference<RenderCommandBuffer>& cmd_buffer, const Reference<Pipeline>& pipeline,
		const Reference<UniformBufferSet>& ubs, const Reference<StorageBufferSet>& sbs, const Reference<Material>& material,
		const Reference<VertexBuffer>& vb, const Reference<IndexBuffer>& ib, const glm::mat4& transform, uint32_t index_count)
	{
		return renderer_api->render_geometry(cmd_buffer, pipeline, ubs, sbs, material, vb, ib, transform, index_count);
	}

	void Renderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer, const Reference<Pipeline>& pipeline,
		const Reference<UniformBufferSet>& uniformBufferSet, const Reference<Material>& material)
	{
		return renderer_api->submit_fullscreen_quad(command_buffer, pipeline, uniformBufferSet, material);
	}

	void Renderer::submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer, const Reference<Pipeline>& pipeline,
		const Reference<UniformBufferSet>& uniformBufferSet, const Reference<StorageBufferSet>& sb, const Reference<Material>& material)
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

	void Renderer::register_shader_dependency(const Reference<Shader>& shader, Reference<ComputePipeline> compute)
	{
		shader_dependencies[shader->get_hash()].computations.push_back(compute);
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

	void Renderer::acknowledge_parsed_global_macros(const std::unordered_set<std::string>& macros, Reference<Shader> shader)
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
		if (renderer_data.global_shader_macros.find(name) != renderer_data.global_shader_macros.end()) {
			if (renderer_data.global_shader_macros.at(name) == value)
				return;
		}

		renderer_data.global_shader_macros[name] = value;

		if (global_shaders.shader_global_macros_map.find(name) == global_shaders.shader_global_macros_map.end()) {
			CORE_WARN("No shaders with {} macro found", name);
			return;
		}

		core_assert(global_shaders.shader_global_macros_map.find(name) != global_shaders.shader_global_macros_map.end(),
			"Macro has not been passed from any shader!");
		for (auto& [hash, shader] : global_shaders.shader_global_macros_map.at(name)) {
			core_assert(shader.is_valid(), "Shader is deleted!");
			global_shaders.dirty_shaders.emplace(shader);
		}
	}

	bool Renderer::update_dirty_shaders()
	{
		const bool updated_any_shader = global_shaders.dirty_shaders.size();
		for (auto shader : global_shaders.dirty_shaders) {
			core_assert(shader.is_valid(), "Shader is deleted!");
			shader->rt_reload(true);
		}
		global_shaders.dirty_shaders.clear();

		return updated_any_shader;
	}

	const std::unordered_map<std::string, std::string>& Renderer::get_global_shader_macros() { return renderer_data.global_shader_macros; }
	// end Registrations

	RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index) { return resource_free_queue[index]; }

	RenderCommandQueue& Renderer::command_queue()
	{
		static RenderCommandQueue* cmd;

		if (!cmd) {
			cmd = new RenderCommandQueue();
		}

		return *cmd;
	}

	RendererConfig& Renderer::get_config() { return config; }

	Reference<RendererContext> Renderer::get_context() { return Application::the().get_window().get_context(); }

	Reference<ShaderLibrary>& Renderer::get_shader_library() { return renderer_data.shader_library; }

	uint32_t Renderer::get_current_frame_index() { return Application::the().get_window().get_swapchain().get_current_buffer_index(); }

	Reference<Texture2D> Renderer::get_white_texture() { return renderer_data.white_texture; }

	Reference<Texture2D> Renderer::get_black_texture() { return renderer_data.black_texture; }

	Reference<Texture2D> Renderer::get_brdf_lut() { return renderer_data.brdf_lut; }

	Reference<TextureCube> Renderer::get_black_cube() { return renderer_data.black_cube; }

	Reference<Texture2D> Renderer::get_hilbert_lut() { return renderer_data.hilbert_lut; }

} // namespace ForgottenEngine
