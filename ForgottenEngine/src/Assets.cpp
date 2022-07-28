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

}
