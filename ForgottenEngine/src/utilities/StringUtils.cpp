#include "fg_pch.hpp"

#include "utilities/StringUtils.hpp"

#include <regex>

namespace ForgottenEngine::StringUtils {

	static int skip_bom(std::istream& stream)
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

		auto path = Assets::load(filepath, "shaders", AssetModifiers::INPUT | AssetModifiers::BINARY);
		if (!path) {
			CORE_ERROR("Could not load file at path: {}", filepath.string());
		}

		std::ifstream& in = *path;
		if (in) {
			in.seekg(0, std::ios::end);
			auto fileSize = in.tellg();
			const int skippedChars = skip_bom(in);

			fileSize -= skippedChars - 1;
			result.resize(fileSize);
			in.read(result.data() + 1, fileSize);
			// Add a dummy tab to beginning of file.
			result[0] = '\t';
		}
		in.close();
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
