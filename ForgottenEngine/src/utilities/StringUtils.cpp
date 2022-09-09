#include "fg_pch.hpp"

#include "utilities/StringUtils.hpp"

#include <regex>

namespace ForgottenEngine::StringUtils {

	static size_t skip_bom(std::istream& stream)
	{
		char test[4] = { 0 };
		stream.seekg(0, std::ios::beg);
		stream.read(test, 3);
		if (strcmp(test, "\xEF\xBB\xBF") == 0) {
			stream.seekg(3, std::ios::beg);
			return 3;
		}
		stream.seekg(0, std::ios::beg);
		return 0;
	}

	// Returns an empty string when failing.
	std::string read_file_and_skip_bom(const std::filesystem::path& filepath)
	{
		std::string result;

		auto path = Assets::in(filepath, "shaders", std::ios::binary | std::ios::ate | std::ios::in);
		if (!path) {
			CORE_ERROR("Could not load file at path: {}", filepath.string());
		}

		std::ifstream& in = *path;
		CORE_ASSERT(in, "Could not open shader file.");

		size_t file_size = in.tellg();
		in.seekg(0);

		result.resize(file_size);
		in.read(result.data(), file_size);
		return result;
	}

	std::vector<std::string> split_string_keep_delims(std::string str)
	{

		const static std::regex re(R"((^\W|^\w+)|(\w+)|[:()])", std::regex_constants::optimize);

		std::regex_iterator<std::string::iterator> rit(str.begin(), str.end(), re);
		std::regex_iterator<std::string::iterator> rend;
		std::vector<std::string> result;

		while (rit != rend) {
			result.emplace_back(rit->str());
			++rit;
		}
		return result;
	}

} // namespace ForgottenEngine::StringUtils
