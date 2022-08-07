#pragma once

#include "render/Shader.hpp"

#include "Hazel/Serialization/Serialization.h"
#include "Hazel/Serialization/ShaderPackFile.h"

#include <filesystem>

namespace ForgottenEngine {

class ShaderPack : public ReferenceCounter {
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
	ShaderPackFile m_File;
	std::filesystem::path m_Path;
};

}
