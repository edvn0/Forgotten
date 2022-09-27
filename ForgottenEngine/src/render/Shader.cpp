#include "fg_pch.hpp"

#include "render/Shader.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/compiler/VulkanShaderCompiler.hpp"
#include "vulkan/VulkanShader.hpp"

#include <utility>

namespace ForgottenEngine {

	Reference<Shader> Shader::create(const std::string& filepath, bool force_compile, bool disable_optimizations)
	{
		Reference<Shader> result = nullptr;

		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			result = Reference<VulkanShader>::create(filepath, force_compile, disable_optimizations);
			break;
		}
		return result;
	}

	ShaderLibrary::ShaderLibrary() = default;

	ShaderLibrary::~ShaderLibrary() = default;

	void ShaderLibrary::add(const std::string& name, const Reference<Shader>& shader)
	{
		core_assert(is_in_map(shaders, name), "Could not find shader with name: {}", name);
		shaders[name] = shader;
	}

	void ShaderLibrary::load(std::string_view path, bool force_compile, bool disable_optimizations)
	{
		Reference<Shader> shader;

		auto found_path = Assets::find_resources_by_path(path, "shaders");
		core_assert(found_path, "Could not find a shader at: {}", *found_path);

		if (!force_compile && shader_pack) {
			if (shader_pack->contains(path))
				shader = shader_pack->load_shader((*found_path).string());
		} else {
			// Try to compile from source
			// Unavailable at runtime

			shader = VulkanShaderCompiler::compile(*found_path, force_compile, disable_optimizations);
		}

		auto& name = shader->get_name();
		core_assert(!is_in_map(shaders, name), "Shader with name [{}] already linked", name);
		shaders[name] = shader;
	}

	void ShaderLibrary::load(std::string_view name, const std::string& path)
	{
		std::string shader_name(name);
		core_assert(!is_in_map(shaders, shader_name), "Shader with name [{}] already linked", name);
		shaders[shader_name] = Shader::create(path);
	}

	const Reference<Shader>& ShaderLibrary::get(const std::string& name) const
	{
		core_assert(is_in_map(shaders, name), "Could not find shader with name: {}", name);
		return shaders.at(name);
	}

	void ShaderLibrary::load_shader_pack(const std::filesystem::path& path)
	{
		shader_pack = Reference<ShaderPack>::create(path);
		if (!shader_pack->is_loaded()) {
			shader_pack = nullptr;
			CORE_ERROR("Could not load shader pack: {}", path.string());
		}
	}

	ShaderUniform::ShaderUniform(std::string name, const ShaderUniformType type, const uint32_t size, const uint32_t offset)
		: name(std::move(name))
		, type(type)
		, size(size)
		, offset(offset)
	{
	}

} // namespace ForgottenEngine
