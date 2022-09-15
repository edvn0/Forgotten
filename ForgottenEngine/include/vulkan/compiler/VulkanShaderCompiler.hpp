#pragma once

#include "preprocessor/ShaderPreprocessor.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/VulkanShaderResource.hpp"
#include "vulkan/VulkanShaderUtils.hpp"
#include "vulkan/vulkan.h"

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace ForgottenEngine {

	struct StageData {
		std::unordered_set<IncludeData> Headers;
		uint32_t HashValue = 0;
		bool operator==(const StageData& other) const noexcept { return this->Headers == other.Headers && this->HashValue == other.HashValue; }
		bool operator!=(const StageData& other) const noexcept { return !(*this == other); }
	};

	class VulkanShaderCompiler : public ReferenceCounted {
	public:
		VulkanShaderCompiler(const std::filesystem::path& shader_source_path, bool disableOptimization = false);

		bool reload(bool forceCompile = false);

		const std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& get_spirv_data() const { return spirv_data; }
		const std::unordered_set<std::string>& get_acknowledged_macros() const { return acknowledged_macros; }

		static void clear_uniform_buffers();

		static Reference<VulkanShader> compile(
			const std::filesystem::path& shader_source_path, bool forceCompile = false, bool disableOptimization = false);
		static bool try_recompile(Reference<VulkanShader> shader);

	private:
		std::unordered_map<VkShaderStageFlagBits, std::string> pre_process(const std::string& source);
		std::unordered_map<VkShaderStageFlagBits, std::string> pre_process_glsl(const std::string& source);

		struct CompilationOptions {
			bool GenerateDebugInfo = false;
			bool Optimize = true;
		};

		std::string compile(std::vector<uint32_t>& outputBinary, const VkShaderStageFlagBits stage, CompilationOptions options) const;

		bool compile_or_get_vulkan_binaries(std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBinary,
			std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBinary, const VkShaderStageFlagBits changedStages,
			const bool forceCompile);

		bool compile_or_get_vulkan_binary(
			VkShaderStageFlagBits stage, std::vector<uint32_t>& outputBinary, bool debug, VkShaderStageFlagBits changedStages, bool forceCompile);

		void clear_reflection_data();
		void serialize_reflection_data();
		void serialize_reflection_data(StreamWriter* serializer);

		void try_get_vulkan_cached_binary(
			const std::filesystem::path& cacheDirectory, const std::string& extension, std::vector<uint32_t>& outputBinary) const;

		bool try_read_cached_reflection_data();

		void reflect_all_shader_stages(const std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);

		void reflect(VkShaderStageFlagBits shaderStage, const std::vector<uint32_t>& shaderData);

	private:
		std::filesystem::path shader_source_path;
		bool disable_optimization = false;

		std::unordered_map<VkShaderStageFlagBits, std::string> shader_source;
		std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>> spirv_debug_data, spirv_data;

		// Reflection info
		VulkanShader::ReflectionData reflection_data;

		std::unordered_map<VkShaderStageFlagBits, StageData> stages_metadata;

		std::unordered_set<std::string> acknowledged_macros;

		ShaderUtils::SourceLang language;

		friend class VulkanShader;
		friend class VulkanShaderCache;
	};

} // namespace ForgottenEngine
