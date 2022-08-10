#include "fg_pch.hpp"

#include "utilities/FileSystem.hpp"

namespace ForgottenEngine {

	bool FileSystem::exists(const std::filesystem::path& filepath) { return std::filesystem::exists(filepath); }

	bool FileSystem::exists(const std::string& filepath)
	{
		return std::filesystem::exists(std::filesystem::path(filepath));
	}

	bool FileSystem::is_directory(const std::filesystem::path& filepath)
	{
		return std::filesystem::is_directory(filepath);
	}

} // namespace ForgottenEngine
