//
//  Assets.cpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-25.
//

#include "fg_pch.hpp"

#include "Assets.hpp"
#include <sys/stat.h>

namespace ForgottenEngine {

OptionalIFStream Assets::load(const Path& path, FileModifier modifier)
{
	const auto found_path = find_resources_by_path(path);

	if (found_path) {
		return std::ifstream(*found_path, modifier);
	}

	return {};
}

bool Assets::exists(const Path& path)
{
	struct stat buffer { };
	return (stat(path.c_str(), &buffer) == 0);
}

OptionalPath Assets::find_resources_by_path(const Path& path)
{
	if (exists(path))
		return path;

	const auto parent_resource_path = path.parent_path() / RESOURCES / path.filename();
	if (exists(parent_resource_path)) {
		return parent_resource_path;
	}

	if (exists(RESOURCES / path)) {
		return RESOURCES / path;
	}

	return {};
}

template <typename T>
static auto load_from_directory_impl = [](const T& a, std::vector<OptionalPath>& out) -> void {
	for (const auto& dir_entry : a) {
		bool is_font_file = dir_entry.path().extension() == ".ttf" || dir_entry.path().extension() == ".otf";
		bool is_regular_and_font_file = dir_entry.is_regular_file() && is_font_file;
		if (is_regular_and_font_file) {
			out.emplace_back(dir_entry.path());
		}
	}
};

std::vector<OptionalPath> Assets::load_from_directory(const std::filesystem::path& path, bool recurse)
{
	std::vector<OptionalPath> result;
	result.reserve(30);

	auto recursive_iterator = std::filesystem::recursive_directory_iterator{ path };
	auto directory_iterator = std::filesystem::directory_iterator{ path };

	if (recurse) {
		load_from_directory_impl<std::filesystem::recursive_directory_iterator>(recursive_iterator, result);
	} else {
		load_from_directory_impl<std::filesystem::directory_iterator>(directory_iterator, result);
	}

	return result;
}

std::string Assets::path_without_extensions(const std::string& input, const std::vector<std::string>& exceptions)
{
	Path path = input;

	auto short_circuit = [&exceptions](const Path& p) {
		for (const auto& exception : exceptions) {
			if (p.extension() == exception) {
				return true;
			}
		}
		return false;
	};

	if (short_circuit(path)) {
		return path.string();
	}

	while (path.has_extension()) {
		path = path.replace_extension();

		if (short_circuit(path)) {
			return path.string();
		}
	}

	return path;
}

}
