#pragma once

#include <vulkan/vulkan.h>

#include "Shader.hpp"

namespace ForgottenEngine {

bool load_vulkan_shader_module(const std::string& path, VkShaderModule* out_module);

class VulkanShader : public Shader {
public:
	~VulkanShader() override = default;

private:
	VkShaderModule shader{ nullptr };

private:
	bool load_shader_from_file(std::string path) override { return load_vulkan_shader_module(path, &shader); }

public:
	VkShaderModule get_module() override;
	explicit VulkanShader(std::string path);
};

}