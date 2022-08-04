#include "fg_pch.hpp"

#include "render/Shader.hpp"

#include <utility>

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanShader.hpp"

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

#define FIND_IN_SHADERS(x) CORE_ASSERT(shaders.find(x) == shaders.end(), "Shader with that name already exists.")

void ShaderLibrary::add(
	const std::string& name, const Reference<Shader>& vertex_shader, const Reference<Shader>& fragment_shader)
{
	FIND_IN_SHADERS(name);
	shaders[name] = { vertex_shader, fragment_shader };
}

void ShaderLibrary::load(std::string_view path, bool force_compile, bool disable_optimizations)
{
	auto vert_path = std::string{ path } + ".vert";
	auto frag_path = std::string{ path } + ".frag";

	ShaderPair shader_pair = { Shader::create(vert_path, force_compile, disable_optimizations),
		Shader::create(frag_path, force_compile, disable_optimizations) };

	auto& name = shader_pair.first->get_name();
	auto stored_name = Assets::path_without_extensions(name);

	FIND_IN_SHADERS(stored_name);
	shaders[stored_name] = shader_pair;
}

void ShaderLibrary::load(std::string_view name, const std::string& path)
{
	auto vert_path = std::string{ path } + ".vert";
	auto frag_path = std::string{ path } + ".frag";

	ShaderPair shader_pair = { Shader::create(vert_path), Shader::create(frag_path) };

	auto stored_name = Assets::path_without_extensions(std::string{ name }, { ".vert", ".frag" });

	FIND_IN_SHADERS(stored_name);
	shaders[stored_name] = shader_pair;
}

const ShaderPair& ShaderLibrary::get(const std::string& name) const
{
	CORE_ASSERT(shaders.find(name) != shaders.end(), "");
	return shaders.at(name);
}

ShaderUniform::ShaderUniform(
	std::string name, const ShaderUniformType type, const uint32_t size, const uint32_t offset)
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

}
