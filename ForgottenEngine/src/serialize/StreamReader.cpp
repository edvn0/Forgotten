#include "fg_pch.hpp"

#include "serialize/StreamReader.hpp"

namespace ForgottenEngine {

void StreamReader::read_buffer(Buffer& buffer, uint32_t size)
{
	buffer.size = size;
	if (size == 0)
		read_data((char*)&buffer.size, sizeof(uint32_t));

	buffer.allocate(buffer.size);
	read_data((char*)buffer.data, buffer.size);
}

void StreamReader::read_string(std::string& string)
{
	size_t size;
	read_data((char*)&size, sizeof(size_t));

	string.resize(size);
	read_data((char*)string.data(), sizeof(char) * size);
}

} // namespace ForgottenEngine
