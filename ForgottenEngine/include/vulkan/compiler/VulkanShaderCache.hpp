#pragma once

#include "vulkan/compiler/VulkanShaderCompiler.hpp"

#include <filesystem>
#include <map>

namespace ForgottenEngine {

	class VulkanShaderCache {
	public:
		static VkShaderStageFlagBits has_changed(Reference<VulkanShaderCompiler> shader);

	private:
		static void serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shader_cache);
		static void deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shader_cache);
	};

} // namespace ForgottenEngine
