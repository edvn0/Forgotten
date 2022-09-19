#include "fg_pch.hpp"

#include "vulkan/compiler/VulkanShaderCache.hpp"

#include "utilities/SerializationMacros.hpp"
#include "vulkan/VulkanShaderUtils.hpp"
#include "vulkan/compiler/preprocessor/ShaderPreprocessor.hpp"
#include "yaml-cpp/yaml.h"

namespace ForgottenEngine {

	static std::filesystem::path cache_path = Assets::slashed_string_to_filepath("shaders/cache/shader_registry.cache");

	VkShaderStageFlagBits VulkanShaderCache::has_changed(Reference<VulkanShaderCompiler> shader)
	{
		std::unordered_map<std::string, std::unordered_map<VkShaderStageFlagBits, StageData>> shader_cache;

		deserialize(shader_cache);

		VkShaderStageFlagBits changed_stages = {};
		const bool shader_not_cached = shader_cache.find(shader->shader_source_path.string()) == shader_cache.end();

		for (const auto& [stage, stage_source] : shader->shader_source) {
			// Keep in mind that we're using the [] operator.
			// Which means that we add the stage if it's not already there.
			if (shader_not_cached || shader->stages_metadata.at(stage) != shader_cache[shader->shader_source_path.string()][stage]) {
				shader_cache[shader->shader_source_path.string()][stage] = shader->stages_metadata.at(stage);
				*(int*)&changed_stages |= stage;
			}
		}

		// Update cache in case we added a stage but didn't remove the deleted(in file) stages
		shader_cache[shader->shader_source_path.string()] = shader->stages_metadata;

		if (changed_stages) {
			serialize(shader_cache);
		}

		return changed_stages;
	}

	void VulkanShaderCache::serialize(const std::unordered_map<std::string, std::unordered_map<VkShaderStageFlagBits, StageData>>& shader_cache)
	{
		YAML::Emitter out;

		out << YAML::BeginMap << YAML::Key << "ShaderRegistry" << YAML::BeginSeq; // ShaderRegistry_

		for (auto& [filepath, shader] : shader_cache) {
			out << YAML::BeginMap; // Shader_

			out << YAML::Key << "ShaderPath" << YAML::Value << filepath;

			out << YAML::Key << "Stages" << YAML::BeginSeq; // Stages_

			for (auto& [stage, stageData] : shader) {
				if (stage == VK_SHADER_STAGE_ALL)
					continue;

				out << YAML::BeginMap; // Stage_

				out << YAML::Key << "Stage" << YAML::Value << ShaderUtils::shader_stage_to_string(stage);
				out << YAML::Key << "StageHash" << YAML::Value << stageData.HashValue;

				out << YAML::Key << "Headers" << YAML::BeginSeq; // Headers_
				for (auto& header : stageData.Headers) {

					out << YAML::BeginMap;

					FG_SERIALIZE_PROPERTY("HeaderPath", header.IncludedFilePath.string(), out);
					FG_SERIALIZE_PROPERTY("IncludeDepth", header.IncludeDepth, out);
					FG_SERIALIZE_PROPERTY("IsRelative", header.IsRelative, out);
					FG_SERIALIZE_PROPERTY("IsGuarded", header.IsGuarded, out);
					FG_SERIALIZE_PROPERTY("HashValue", header.HashValue, out);

					out << YAML::EndMap;
				}
				out << YAML::EndSeq; // Headers_

				out << YAML::EndMap; // Stage_
			}
			out << YAML::EndSeq; // Stages_
			out << YAML::EndMap; // Shader_
		}
		out << YAML::EndSeq; // ShaderRegistry_
		out << YAML::EndMap; // File_

		std::ofstream output(Assets::get_base_directory() / cache_path);

		if (!output) {
			CORE_ERROR("Could not open {} output path.", Assets::get_base_directory() / cache_path);
			return;
		}

		output << out.c_str();
	}

	void VulkanShaderCache::deserialize(std::unordered_map<std::string, std::unordered_map<VkShaderStageFlagBits, StageData>>& shader_cache)
	{
		// Read registry
		std::ifstream stream(Assets::get_base_directory() / cache_path);
		if (!stream) {
			CORE_ERROR("Could not load cache directory at {}", cache_path);
			return;
		}

		std::stringstream str_stream;
		str_stream << stream.rdbuf();

		YAML::Node data = YAML::Load(str_stream.str());
		auto handles = data["ShaderRegistry"];
		if (handles.IsNull()) {
			CORE_ERROR("[ShaderCache] Shader Registry is invalid.");
			return;
		}

		// Old format
		if (handles.IsMap()) {
			CORE_ERROR("[ShaderCache] Old Shader Registry format.");
			return;
		}

		for (auto shader : handles) {
			std::string path;
			FG_DESERIALIZE_PROPERTY("ShaderPath", path, shader, std::string());
			for (auto stage : shader["Stages"]) // Stages
			{
				std::string stage_type;
				uint32_t stage_hash;
				FG_DESERIALIZE_PROPERTY("Stage", stage_type, stage, std::string());
				FG_DESERIALIZE_PROPERTY("StageHash", stage_hash, stage, 0u);

				auto& stage_cache = shader_cache[path][ShaderUtils::ShaderTypeFromString(stage_type)];
				stage_cache.HashValue = stage_hash;

				for (auto header : stage["Headers"]) {
					std::string header_path;
					uint32_t include_depth;
					bool is_relative;
					bool is_guarded;
					uint32_t hash_value;
					FG_DESERIALIZE_PROPERTY("HeaderPath", header_path, header, std::string());
					FG_DESERIALIZE_PROPERTY("IncludeDepth", include_depth, header, 0u);
					FG_DESERIALIZE_PROPERTY("IsRelative", is_relative, header, false);
					FG_DESERIALIZE_PROPERTY("IsGuarded", is_guarded, header, false);
					FG_DESERIALIZE_PROPERTY("HashValue", hash_value, header, 0u);

					stage_cache.Headers.emplace(IncludeData { header_path, include_depth, is_relative, is_guarded, hash_value });
				}
			}
		}
	}

} // namespace ForgottenEngine
