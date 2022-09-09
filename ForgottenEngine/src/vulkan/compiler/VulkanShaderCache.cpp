#include "fg_pch.hpp"

#include "vulkan/compiler/VulkanShaderCache.hpp"

#include "utilities/SerializationMacros.hpp"
#include "vulkan/VulkanShaderUtils.hpp"
#include "vulkan/compiler/preprocessor/ShaderPreprocessor.hpp"
#include "yaml-cpp/yaml.h"

namespace ForgottenEngine {

	static std::filesystem::path cache_path = Assets::slashed_string_to_filepath("resources/shaders/cache/shader_registry.cache");

	VkShaderStageFlagBits VulkanShaderCache::has_changed(Reference<VulkanShaderCompiler> shader)
	{
		std::map<std::string, std::map<VkShaderStageFlagBits, StageData>> shader_cache;

		deserialize(shader_cache);

		VkShaderStageFlagBits changed_stages = {};
		const bool shaderNotCached = shader_cache.find(shader->shader_source_path.string()) == shader_cache.end();

		for (const auto& [stage, stage_source] : shader->shader_source) {
			// Keep in mind that we're using the [] operator.
			// Which means that we add the stage if it's not already there.
			if (shaderNotCached || shader->stages_metadata.at(stage) != shader_cache[shader->shader_source_path.string()][stage]) {
				shader_cache[shader->shader_source_path.string()][stage] = shader->stages_metadata.at(stage);
				*(int*)&changed_stages |= stage;
			}
		}

		// Update cache in case we added a stage but didn't remove the deleted(in file) stages
		shader_cache.at(shader->shader_source_path.string()) = shader->stages_metadata;

		if (changed_stages) {
			serialize(shader_cache);
		}

		return changed_stages;
	}

	void VulkanShaderCache::serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shader_cache)
	{
		YAML::Emitter out;

		out << YAML::BeginMap << YAML::Key << "ShaderRegistry" << YAML::BeginSeq; // ShaderRegistry_

		for (auto& [filepath, shader] : shader_cache) {
			out << YAML::BeginMap; // Shader_

			out << YAML::Key << "ShaderPath" << YAML::Value << filepath;

			out << YAML::Key << "Stages" << YAML::BeginSeq; // Stages_

			for (auto& [stage, stageData] : shader) {
				out << YAML::BeginMap; // Stage_

				out << YAML::Key << "Stage" << YAML::Value << ShaderUtils::ShaderStageToString(stage);
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

		std::ofstream fout(*Assets::out(cache_path));
		fout << out.c_str();
	}

	void VulkanShaderCache::deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shader_cache)
	{
		// Read registry
		auto asset_path = *Assets::in(cache_path);
		std::ifstream stream(std::move(asset_path));
		if (!stream) {
			CORE_ERROR("Could not load cache directory at {}", cache_path);
			return;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
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
				std::string stageType;
				uint32_t stageHash;
				FG_DESERIALIZE_PROPERTY("Stage", stageType, stage, std::string());
				FG_DESERIALIZE_PROPERTY("StageHash", stageHash, stage, 0u);

				auto& stageCache = shader_cache[path][ShaderUtils::ShaderTypeFromString(stageType)];
				stageCache.HashValue = stageHash;

				for (auto header : stage["Headers"]) {
					std::string headerPath;
					uint32_t includeDepth;
					bool isRelative;
					bool isGuarded;
					uint32_t hashValue;
					FG_DESERIALIZE_PROPERTY("HeaderPath", headerPath, header, std::string());
					FG_DESERIALIZE_PROPERTY("IncludeDepth", includeDepth, header, 0u);
					FG_DESERIALIZE_PROPERTY("IsRelative", isRelative, header, false);
					FG_DESERIALIZE_PROPERTY("IsGuarded", isGuarded, header, false);
					FG_DESERIALIZE_PROPERTY("HashValue", hashValue, header, 0u);

					stageCache.Headers.emplace(IncludeData { headerPath, includeDepth, isRelative, isGuarded, hashValue });
				}
			}
		}
	}

} // namespace ForgottenEngine
