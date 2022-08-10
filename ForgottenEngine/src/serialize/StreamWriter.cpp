#include "fg_pch.hpp"

#include "serialize/StreamWriter.hpp"

namespace ForgottenEngine {
	void StreamWriter::write_buffer(Buffer buffer, bool writeSize)
	{
		if (writeSize)
			write_data((char*)&buffer.size, sizeof(uint32_t));

		write_data((char*)buffer.data, buffer.size);
	}

	void StreamWriter::write_zero(uint64_t size)
	{
		char zero = 0;
		for (uint64_t i = 0; i < size; i++)
			write_data(&zero, 1);
	}

	void StreamWriter::write_string(const std::string& string)
	{
		size_t size = string.size();
		write_data((char*)&size, sizeof(size_t));
		write_data((char*)string.data(), sizeof(char) * string.size());
	}

} // namespace ForgottenEngine
