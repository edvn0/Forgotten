#pragma once

#include "render/Shader.hpp"

#include "serialize/Serialization.hpp"
#include "serialize/ShaderPackFile.hpp"

#include <filesystem>

namespace ForgottenEngine {

class ShaderPack : public ReferenceCounted {
public:
	ShaderPack() = default;
	ShaderPack(const std::filesystem::path& path);

	bool is_loaded() const { return loaded; }
	bool contains(std::string_view name) const;

	Reference<Shader> load_shader(std::string_view name);

	static Reference<ShaderPack> create_from_library(
		Reference<ShaderLibrary> shaderLibrary, const std::filesystem::path& path);

private:
	bool loaded = false;
	ShaderPackFile file;
	std::filesystem::path path;
};

}
