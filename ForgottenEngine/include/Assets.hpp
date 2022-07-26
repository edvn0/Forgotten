//
//  AssetLibrary.hpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-24.
//

#pragma once

#include <filesystem>
#include <optional>

#define BIT(x) (1 << x)

namespace ForgottenEngine {

using Path = std::filesystem::path;
using OptionalPath = std::optional<std::filesystem::path>;
using IFStream = std::ifstream;
using OptionalIFStream = std::optional<IFStream>;

using FileModifier = unsigned int;

namespace Asset {
	static const FileModifier APPEND = 0x01;
	static const FileModifier OPEN_AT_END = 0x02;
	static const FileModifier BINARY = 0x04;
	static const FileModifier INPUT = 0x08;
	static const FileModifier OUTPUT = 0x10;
	static const FileModifier TRUNCATE = 0x20;
}

static const Path RESOURCES = "resources";

class Assets {
public:
	static OptionalIFStream load(
		const Path&, FileModifier modifier = Asset::INPUT | Asset::BINARY | Asset::OPEN_AT_END);
	static bool exists(const Path&);
	static OptionalPath find_resources_by_path(const Path&);
};

}
