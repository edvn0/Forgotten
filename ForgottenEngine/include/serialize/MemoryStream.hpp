#pragma once

#include "Buffer.hpp"
#include "serialize/StreamReader.hpp"
#include "serialize/StreamWriter.hpp"

namespace ForgottenEngine {
	//==============================================================================
	/// MemoryStreamWriter
	class MemoryStreamWriter : public StreamWriter {
	public:
		MemoryStreamWriter(Buffer& buffer, size_t size);
		MemoryStreamWriter(const MemoryStreamWriter&) = delete;
		~MemoryStreamWriter();

		bool is_stream_good() const final { return write_pos < buffer.size; }
		uint64_t get_stream_position() final { return write_pos; }
		void set_stream_position(uint64_t position) final { write_pos = position; }
		bool write_data(const char* data, size_t size) final;

	private:
		Buffer& buffer;
		size_t write_pos = 0;
	};

	//==============================================================================
	/// MemoryStreamReader
	class MemoryStreamReader : public StreamReader {
	public:
		MemoryStreamReader(const Buffer& buffer);
		MemoryStreamReader(const MemoryStreamReader&) = delete;
		~MemoryStreamReader();

		bool is_stream_good() const final { return read_pos < buffer.size; }
		uint64_t get_stream_position() final { return read_pos; }
		void set_stream_position(uint64_t position) final { read_pos = position; }
		bool read_data(char* destination, size_t size) final;

	private:
		const Buffer& buffer;
		size_t read_pos = 0;
	};

} // namespace ForgottenEngine
