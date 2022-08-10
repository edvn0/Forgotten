#pragma once

#include <filesystem>
#include <string>

namespace ForgottenEngine::StringUtils {

	std::string read_file_and_skip_bom(const std::filesystem::path& path);
	std::string read_file_and_skip_bom(const std::string& path);

	std::vector<std::string> split_string_keep_delims(std::string str);

	// ------ Constexpr ---------------

	constexpr bool starts_with(std::string_view t, std::string_view s)
	{
		auto len = s.length();
		return t.length() >= len && t.substr(0, len) == s;
	}

	constexpr bool ends_with(std::string_view t, std::string_view s)
	{
		auto len1 = t.length(), len2 = s.length();
		return len1 >= len2 && t.substr(len1 - len2) == s;
	}

	constexpr size_t get_number_of_tokens(std::string_view source, std::string_view delimiter)
	{
		size_t count = 1;
		auto pos = source.begin();
		while (pos != source.end()) {
			if (std::string_view(&*pos, delimiter.size()) == delimiter)
				++count;

			++pos;
		}
		return count;
	}

	template <size_t N>
	constexpr std::array<std::string_view, N> split_string(std::string_view source, std::string_view delimiter)
	{
		std::array<std::string_view, N> tokens;

		auto tokenStart = source.begin();
		auto pos = tokenStart;

		size_t i = 0;

		while (pos != source.end()) {
			if (std::string_view(&*pos, delimiter.size()) == delimiter) {
				tokens[i] = std::string_view(&*tokenStart, (pos - tokenStart));
				tokenStart = pos += delimiter.size();
				++i;
			} else {
				++pos;
			}
		}

		if (pos != source.begin())
			tokens[N - 1] = std::string_view(&*tokenStart, (pos - tokenStart));

		return tokens;
	}

	static constexpr std::string_view remove_namespace(std::string_view name)
	{
		const auto pos = name.find_last_of(':');
		if (pos == std::string_view::npos)
			return name;

		return name.substr(name.find_last_of(':') + 1);
	}

	static constexpr std::string_view remove_outer_namespace(std::string_view name)
	{
		const auto first = name.find_first_of(':');
		if (first == std::string_view::npos)
			return name;

		if (first < name.size() - 1 && name[first + 1] == ':')
			return name.substr(first + 2);
		else
			return name.substr(first + 1);
	}

	template <size_t N>
	static constexpr std::array<std::string_view, N>& remove_namespace(std::array<std::string_view, N>& memberList)
	{
		for (std::string_view& fullName : memberList)
			fullName = remove_namespace(fullName);

		return memberList;
	}

} // namespace ForgottenEngine::StringUtils
