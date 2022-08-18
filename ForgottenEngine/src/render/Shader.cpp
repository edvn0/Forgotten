#include "fg_pch.hpp"

#include "render/Shader.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/compiler/VulkanShaderCompiler.hpp"

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
		CORE_ASSERT_BOOL(shaders.find(name) == shaders.end());
		shaders[name] = shader;
	}

	void ShaderLibrary::load(std::string_view path, bool force_compile, bool disable_optimizations)
	{
		Reference<Shader> shader;

		auto found_path = Assets::find_resources_by_path(path, "shaders");
		CORE_ASSERT_BOOL(found_path);

		if (!force_compile && shader_pack) {
			if (shader_pack->contains(path))
				shader = shader_pack->load_shader((*found_path).string());
		} else {
			// Try to compile from source
			// Unavailable at runtime
			shader = VulkanShaderCompiler::compile(*found_path, force_compile, disable_optimizations);
		}

		auto& name = shader->get_name();
		CORE_ASSERT_BOOL(shaders.find(name) == shaders.end());
		shaders[name] = shader;
	}

	void ShaderLibrary::load(std::string_view name, const std::string& path)
	{
		std::string shader_name(name);
		CORE_ASSERT_BOOL(shaders.find(shader_name) != shaders.end());
		shaders[shader_name] = Shader::create(path);
	}

	const Reference<Shader>& ShaderLibrary::get(const std::string& name) const
	{
		CORE_ASSERT_BOOL(shaders.find(name) != shaders.end());
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

	constexpr std::string_view ShaderUniform::uniform_type_to_string(const ShaderUniformType type)
	{
		if (type == ShaderUniformType::Bool) {
			return "Boolean";
		} else if (type == ShaderUniformType::Int) {
			return "Int";
		} else if (type == ShaderUniformType::Float) {
			return "Float";
		}

		return "None";
	}

} // namespace ForgottenEngine
