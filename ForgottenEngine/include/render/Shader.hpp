#pragma once

#include "Buffer.hpp"
#include "Common.hpp"

#include "render/ShaderUniform.hpp"

#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace ForgottenEngine {
namespace ShaderUtils {
	enum class SourceLang {
		NONE,
		GLSL,
		HLSL,
	};
}

enum class ShaderUniformType {
	None = 0,
	Bool,
	Int,
	UInt,
	Float,
	Vec2,
	Vec3,
	Vec4,
	Mat3,
	Mat4,
	IVec2,
	IVec3,
	IVec4
};

class ShaderUniform {
public:
	ShaderUniform() = default;
	ShaderUniform(std::string name, ShaderUniformType type, uint32_t size, uint32_t offset);

	[[nodiscard]] const std::string& get_name() const { return name; }
	[[nodiscard]] ShaderUniformType get_type() const { return type; }
	[[nodiscard]] uint32_t get_size() const { return size; }
	[[nodiscard]] uint32_t get_offset() const { return offset; }

	static constexpr std::string_view uniform_type_to_string(ShaderUniformType type);

private:
	std::string name;
	ShaderUniformType type = ShaderUniformType::None;
	uint32_t size = 0;
	uint32_t offset = 0;
};

struct ShaderUniformBuffer {
	std::string Name;
	uint32_t Index;
	uint32_t BindingPoint;
	uint32_t Size;
	uint32_t RendererID;
	std::vector<ShaderUniform> Uniforms;
};

struct ShaderStorageBuffer {
	std::string Name;
	uint32_t Index;
	uint32_t BindingPoint;
	uint32_t Size;
	uint32_t RendererID;
	// std::vector<ShaderUniform> Uniforms;
};

struct ShaderBuffer {
	std::string Name;
	uint32_t Size = 0;
	std::unordered_map<std::string, ShaderUniform> Uniforms;
};

class Shader : public ReferenceCounted {
public:
	using ShaderReloadedCallback = std::function<void()>;

    virtual ~Shader() = default;

	virtual void reload(bool force_compile = false) = 0;
	virtual void rt_reload(bool force_compile) = 0;

	virtual size_t get_hash() const = 0;

	virtual const std::string& get_name() const = 0;

	virtual void set_macro(const std::string& name, const std::string& value) = 0;

	static Reference<Shader> create(
		const std::string& filepath, bool force_compile = false, bool disable_optimisations = false);

	virtual const std::unordered_map<std::string, ShaderBuffer>& get_shader_buffers() const = 0;
	virtual const std::unordered_map<std::string, ShaderResourceDeclaration>& get_resources() const = 0;

	virtual void add_shader_reloaded_callback(const ShaderReloadedCallback& callback) = 0;
};

// This should be eventually handled by the Asset Manager
class ShaderLibrary : public ReferenceCounted {
public:
	ShaderLibrary();
	~ShaderLibrary();

	void add(const Reference<Shader>& shader);
	void load(std::string_view path, bool force_compile = false, bool disable_optimisations = false);
	void load(std::string_view name, const std::string& path);

	const Reference<Shader>& get(const std::string& name) const;
	size_t get_size() const { return shaders.size(); }

	std::unordered_map<std::string, Reference<Shader>>& get_shaders() { return shaders; }
	const std::unordered_map<std::string, Reference<Shader>>& get_shaders() const { return shaders; }

private:
	std::unordered_map<std::string, Reference<Shader>> shaders;
};

}
