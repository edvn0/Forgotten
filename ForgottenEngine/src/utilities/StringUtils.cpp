#include "fg_pch.hpp"

#include "utilities/StringUtils.hpp"

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
	std::ifstream in(filepath, std::ios::in | std::ios::binary);
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

} // namespace ForgottenEngine::StringUtils
