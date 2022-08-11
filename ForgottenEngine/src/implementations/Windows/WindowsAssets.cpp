#include "fg_pch.hpp"

#include "Assets.hpp"

#include <Windows.h>

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
		CORE_ASSERT(initialized, "");
		return working_directory;
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

	bool Assets::exists(const Path& p)
	{
		return std::filesystem::exists(p);
	}

} // namespace ForgottenEngine
