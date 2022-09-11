#include "fg_pch.hpp"

#include "Assets.hpp"

#include <Windows.h>
#include <regex>

namespace ForgottenEngine {

	static std::filesystem::path working_directory;
	static bool initialized = false;

	void Assets::init()
	{
		working_directory = std::filesystem::current_path();
		initialized = true;
	}

	Path Assets::get_base_directory()
	{
		CORE_ASSERT_BOOL(initialized);
		return working_directory;
	}

	Path Assets::slashed_string_to_filepath(const std::string& slashed_string)
	{
		auto vector = [&slashed_string]() {
			std::stringstream stream(slashed_string);
			std::string item;
			std::vector<std::string> split_strings;
			while (std::getline(stream, item, '/')) {
				split_strings.push_back(item);
			}
			return split_strings;
		}();

		std::filesystem::path result;
		for (auto&& subpath : vector) {
			result /= subpath;
		}

		return result;
	}

	OptionalPath Assets::find_resources_by_path(const Path& path, const std::string& subdirectory)
	{
		if (exists(path))
			return path;

		std::filesystem::path try_path;
		if (subdirectory.empty())
			try_path = working_directory / RESOURCES / path.filename();
		else
			try_path = working_directory / RESOURCES / std::filesystem::path(subdirectory) / path.filename();

		if (exists(try_path)) {
			return try_path;
		}

		if (exists(RESOURCES / path)) {
			return RESOURCES / path;
		}

		return {};
	}

	bool Assets::exists(const Path& p) { return std::filesystem::exists(p); }

	const char* Assets::c_str(const Path& path) { return path.string().c_str(); }

} // namespace ForgottenEngine
