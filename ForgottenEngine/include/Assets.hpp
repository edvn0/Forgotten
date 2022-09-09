//
//  AssetLibrary.hpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-24.
//

#pragma once

#include "Common.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace ForgottenEngine {

	using Path = std::filesystem::path;
	using OptionalPath = std::optional<std::filesystem::path>;
	using IFStream = std::ifstream;
	using OFStream = std::ofstream;
	using OptionalIFStream = std::optional<IFStream>;
	using OptionalOFStream = std::optional<OFStream>;

	using FileModifier = int;

	namespace AssetModifiers {
		static const FileModifier APPEND = 0x01;
		static const FileModifier OPEN_AT_END = 0x02;
		static const FileModifier BINARY = 0x04;
		static const FileModifier INPUT = 0x08;
		static const FileModifier OUTPUT = 0x10;
		static const FileModifier TRUNCATE = 0x20;
	} // namespace AssetModifiers

	static const Path RESOURCES = "resources";

	class Assets {
	public:
		static void init();

		static Path get_base_directory();

		static Path slashed_string_to_filepath(const std::string& slashed_string);

		/**
		 * @brief Optional resolution of an input stream based on a path.
		 *
		 * @param modifier {@link std::ios @endlink }
		 * @return OptionalIFStream std::optional<std::ifstream>>
		 */
		static OptionalIFStream in(const Path&, FileModifier modifier = AssetModifiers::INPUT | AssetModifiers::BINARY | AssetModifiers::OPEN_AT_END);

		/**
		 * @brief Optional resolution of an input stream based on a path and an optional directory in which to look for it.
		 *
		 * @param modifier {@link std::ios @endlink }
		 * @return OptionalIFStream std::optional<std::ifstream>>
		 */
		static OptionalIFStream in(const Path&, const std::string& resource_subdirectory,
			FileModifier modifier = AssetModifiers::INPUT | AssetModifiers::BINARY | AssetModifiers::OPEN_AT_END);

		/**
		 * @brief Optional resolution of an output stream based on a path.
		 *
		 * @param modifier {@link std::ios @endlink }
		 * @return OptionalIFStream std::optional<std::ofstream>>
		 */
		static OptionalOFStream out(
			const Path&, FileModifier modifier = AssetModifiers::INPUT | AssetModifiers::BINARY | AssetModifiers::OPEN_AT_END);

		/**
		 * @brief Optional resolution of an output stream based on a path and an optional directory in which to look for it.
		 *
		 * @param modifier {@link std::ios @endlink }
		 * @return OptionalIFStream std::optional<std::ofstream>>
		 */
		static OptionalOFStream out(const Path&, const std::string& resource_subdirectory,
			FileModifier modifier = AssetModifiers::INPUT | AssetModifiers::BINARY | AssetModifiers::OPEN_AT_END);

		static bool exists(const Path&);
		static OptionalPath find_resources_by_path(const Path&, const std::string& resource_subdirectory = "");
		static std::vector<OptionalPath> load_from_directory(const std::filesystem::path& path, bool recurse = false);

		static std::string path_without_extensions(const std::string& input, const std::vector<std::string>& exceptions = {});

		static const char* c_str(const Path& path);

		static std::string extract_extension(const std::string& input);
	};

} // namespace ForgottenEngine
