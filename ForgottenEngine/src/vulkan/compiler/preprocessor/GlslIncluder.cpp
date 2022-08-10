#include "fg_pch.hpp"

#include "vulkan/compiler/preprocessor/GlslIncluder.hpp"

#include "utilities/StringUtils.hpp"
#include "vulkan/VulkanShader.hpp"

#include <libshaderc_util/io_shaderc.h>

namespace ForgottenEngine {

	GlslIncluder::GlslIncluder(const shaderc_util::FileFinder* file_finder)
		: file_finder(*file_finder)
	{
	}

	GlslIncluder::~GlslIncluder() = default;

	shaderc_include_result* GlslIncluder::GetInclude(const char* requestedPath, const shaderc_include_type type,
		const char* requestingPath, const size_t includeDepth)
	{
		const std::filesystem::path requestedFullPath = (type == shaderc_include_type_relative)
			? file_finder.FindRelativeReadableFilepath(requestingPath, requestedPath)
			: file_finder.FindReadableFilepath(requestedPath);

		auto& [source, sourceHash, stages, isGuarded] = header_cache[requestedFullPath.string()];

		if (source.empty()) {
			source = StringUtils::read_file_and_skip_bom(requestedFullPath);
			if (source.empty())
				CORE_ERROR("Failed to load included file: {} in {}.", requestedFullPath, requestingPath);
			sourceHash = Hash::generate_fnv_hash(source.c_str());

			// Can clear "source" in case it has already been included in this stage and is guarded.
			stages = ShaderPreprocessor::PreprocessHeader<ShaderUtils::SourceLang::GLSL>(
				source, isGuarded, parsed_special_macros, include_data, requestedFullPath);
		} else if (isGuarded) {
			source.clear();
		}

		// Does not emplace if it finds the same include path and same header hash value.
		include_data.emplace(IncludeData {
			requestedFullPath, includeDepth, type == shaderc_include_type_relative, isGuarded, sourceHash, stages });

		auto* const container = new std::array<std::string, 2>;
		(*container)[0] = requestedPath;
		(*container)[1] = source;
		auto* const data = new shaderc_include_result;

		data->user_data = container;

		data->source_name = (*container)[0].data();
		data->source_name_length = (*container)[0].size();

		data->content = (*container)[1].data();
		data->content_length = (*container)[1].size();

		return data;
	}

	void GlslIncluder::ReleaseInclude(shaderc_include_result* data)
	{
		delete static_cast<std::array<std::string, 2>*>(data->user_data);
		delete data;
	}

} // namespace ForgottenEngine
