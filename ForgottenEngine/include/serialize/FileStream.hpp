#pragma once

#include "Buffer.hpp"
#include "serialize/StreamReader.hpp"
#include "serialize/StreamWriter.hpp"

#include <filesystem>
#include <fstream>

namespace ForgottenEngine {
//==============================================================================
/// FileStreamWriter
class FileStreamWriter : public StreamWriter {
public:
	FileStreamWriter(const std::filesystem::path& path);
	FileStreamWriter(const FileStreamWriter&) = delete;
	~FileStreamWriter();

	bool is_stream_good() const final override { return stream.good(); }
	uint64_t get_stream_position() final override { return stream.tellp(); }
	void set_stream_position(uint64_t position) final override { stream.seekp(position); }
	bool write_data(const char* data, size_t size) final override;

private:
	std::filesystem::path path;
	std::ofstream stream;
};

//==============================================================================
/// FileStreamReader
class FileStreamReader : public StreamReader {
public:
	FileStreamReader(const std::filesystem::path& path);
	FileStreamReader(const FileStreamReader&) = delete;
	~FileStreamReader();

	bool is_stream_good() const final override { return stream.good(); }
	uint64_t get_stream_position() final override { return stream.tellg(); }
	void set_stream_position(uint64_t position) final override { stream.seekg(position); }
	bool read_data(char* destination, size_t size) final override;

private:
	std::filesystem::path path;
	std::ifstream stream;
};

} // namespace Hazel
