#include "fg_pch.hpp"

#include "serialize/FileStream.hpp"

namespace ForgottenEngine {
	//==============================================================================
	/// FileStreamWriter
	FileStreamWriter::FileStreamWriter(const std::filesystem::path& path)
		: path(path)
	{
		stream = std::ofstream(path, std::ifstream::out | std::ifstream::binary);
	}

	FileStreamWriter::~FileStreamWriter() { stream.close(); }

	bool FileStreamWriter::write_data(const char* data, size_t size)
	{
		stream.write(data, size);
		return true;
	}

	//==============================================================================
	/// FileStreamReader
	FileStreamReader::FileStreamReader(const std::filesystem::path& path)
		: path(path)
	{
		stream = std::ifstream(path, std::ifstream::in | std::ifstream::binary);
	}

	FileStreamReader::~FileStreamReader() { stream.close(); }

	bool FileStreamReader::read_data(char* destination, size_t size)
	{
		stream.read(destination, size);
		return true;
	}

} // namespace ForgottenEngine
