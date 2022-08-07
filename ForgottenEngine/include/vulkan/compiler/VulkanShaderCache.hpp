#pragma once

#include <filesystem>
#include <map>

#include "vulkan/compiler/VulkanShaderCompiler.hpp"

namespace ForgottenEngine {

class VulkanShaderCache {
public:
	static VkShaderStageFlagBits has_changed(Reference<VulkanShaderCompiler> shader);

private:
	static void serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
	static void deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
};

}
