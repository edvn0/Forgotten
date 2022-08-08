#include "fg_pch.hpp"

#include "serialize/MemoryStream.hpp"

namespace ForgottenEngine {
//==============================================================================
/// MemoryStreamWriter
MemoryStreamWriter::MemoryStreamWriter(Buffer& buffer, size_t size)
	: buffer(buffer)
{
	if (size > buffer.size)
		buffer.allocate((uint32_t)size);
}

MemoryStreamWriter::~MemoryStreamWriter() { }

bool MemoryStreamWriter::write_data(const char* data, size_t size)
{
	if (write_pos + size > buffer.size)
		return false;

	buffer.write(data, (uint32_t)size, (uint32_t)write_pos);
	return true;
}

//==============================================================================
/// MemoryStreamReader
MemoryStreamReader::MemoryStreamReader(const Buffer& buffer)
	: buffer(buffer)
{
}

MemoryStreamReader::~MemoryStreamReader() { }

bool MemoryStreamReader::read_data(char* destination, size_t size)
{
	if (read_pos + size > buffer.size)
		return false;

	memcpy(destination, (char*)buffer.data + read_pos, size);
	return true;
}

} // namespace ForgottenEngine
