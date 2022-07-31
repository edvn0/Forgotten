#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

struct Buffer {
	void* data;
	uint32_t size;

	Buffer()
		: data(nullptr)
		, size(0)
	{
	}

	Buffer(void* data, uint32_t size)
		: data(data)
		, size(size)
	{
	}

	static Buffer copy(const void* data, uint32_t size)
	{
		Buffer buffer;
		buffer.allocate(size);
		memcpy(buffer.data, data, size);
		return buffer;
	}

	void allocate(uint32_t size)
	{
		delete[](byte*) data;
		data = nullptr;

		if (size == 0)
			return;

		data = new byte[size];
		size = size;
	}

	void release()
	{
		delete[](byte*) data;
		data = nullptr;
		size = 0;
	}

	void zero_initialize()
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