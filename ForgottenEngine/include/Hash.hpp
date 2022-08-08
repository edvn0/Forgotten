#pragma once

#include <string>

namespace ForgottenEngine {

class Hash {
public:
	static constexpr uint32_t generate_fnv_hash(const char* str)
	{
		constexpr uint32_t FNV_PRIME = 16777619u;
		constexpr uint32_t OFFSET_BASIS = 2166136261u;

		const size_t length = std::string_view(str).length() + 1;
		uint32_t hash = OFFSET_BASIS;
		for (size_t i = 0; i < length; ++i) {
			hash ^= *str++;
			hash *= FNV_PRIME;
		}
		return hash;
	}

	static constexpr uint32_t generate_fnv_hash(std::string_view string)
	{
		return generate_fnv_hash(string.data());
	}

	static uint32_t crc_32(const char* str);
	static uint32_t crc_32(const std::string& string);
};

}
