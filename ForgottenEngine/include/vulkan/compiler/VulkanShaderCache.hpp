#pragma once

#include "vulkan/compiler/VulkanShaderCompiler.hpp"

#include <filesystem>
#include <unordered_map>

namespace ForgottenEngine {

	class VulkanShaderCache {
	public:
		static VkShaderStageFlagBits has_changed(Reference<VulkanShaderCompiler> shader);

	private:
		static void serialize(const std::unordered_map<std::string, std::unordered_map<VkShaderStageFlagBits, StageData>>& shader_cache);
		static void deserialize(std::unordered_map<std::string, std::unordered_map<VkShaderStageFlagBits, StageData>>& shader_cache);
	};

} // namespace ForgottenEngine
