#pragma once

#include <libshaderc_util/file_finder.h>
#include <shaderc/shaderc.hpp>
#include <unordered_set>

#include "vulkan/compiler/preprocessor/ShaderPreprocessor.hpp"

namespace ForgottenEngine {

struct IncludeData;

class GlslIncluder : public shaderc::CompileOptions::IncluderInterface {
public:
	explicit GlslIncluder(const shaderc_util::FileFinder* file_finder);

	~GlslIncluder() override;

	shaderc_include_result* GetInclude(const char* requestedPath, shaderc_include_type type,
		const char* requestingPath, size_t includeDepth) override;

	void ReleaseInclude(shaderc_include_result* data) override;

	std::unordered_set<IncludeData>&& get_include_data() { return std::move(include_data); }
	std::unordered_set<std::string>&& get_parsed_special_macros() { return std::move(parsed_special_macros); }

private:
	// Used by GetInclude() to get the full filepath.
	const shaderc_util::FileFinder& file_finder;
	std::unordered_set<IncludeData> include_data;
	std::unordered_set<std::string> parsed_special_macros;
	std::unordered_map<std::string, HeaderCache> header_cache;
};
}
