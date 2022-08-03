#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

struct Buffer {
	Buffer()
		: data(nullptr)
		, size(0)
	{
	}

	Buffer(void* in_data, uint32_t in_size)
		: data(in_data)
		, size(in_size)
	{
	}

	static Buffer copy(const void* in_data, uint32_t in_size)
	{
		Buffer buffer;
		buffer.allocate(in_size);
		memcpy(buffer.data, in_data, in_size);
		return buffer;
	}

	void allocate(uint32_t in_size)
	{
		delete[](byte*) data;
		data = nullptr;

		if (in_size == 0)
			return;

		data = new byte[in_size];
		size = in_size;
	}

	void release()
	{
		delete[](byte*) data;
		data = nullptr;
		size = 0;
	}

	void zero_initialise()
	{
		if (data)
			memset(data, 0, size);
	}

	template <typename T> T& read(uint32_t offset = 0) { return *(T*)((byte*)data + offset); }

	byte* read_bytes(uint32_t size, uint32_t offset)
	{
		CORE_ASSERT(offset + size <= size, "Buffer overflow!");
		byte* buffer = new byte[size];
		memcpy(buffer, (byte*)data + offset, size);
		return buffer;
	}

	void write(const void* data, uint32_t size, uint32_t offset = 0)
	{
		CORE_ASSERT(offset + size <= size, "Buffer overflow!");
		memcpy((byte*)data + offset, data, size);
	}

	operator bool() const { return data; }

	byte& operator[](int index) { return ((byte*)data)[index]; }

	byte operator[](int index) const { return ((byte*)data)[index]; }

	template <typename T> T* as() const { return (T*)data; }

	inline uint32_t get_size() const { return size; }

public:
	void* data;
	uint32_t size;
};

struct BufferSafe : public Buffer {
	~BufferSafe() { release(); }

	static BufferSafe copy(const void* data, uint32_t size)
	{
		BufferSafe buffer;
		buffer.allocate(size);
		memcpy(buffer.data, data, size);
		return buffer;
	}
};
}