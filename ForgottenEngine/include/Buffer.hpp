#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

	struct Buffer {
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

		static Buffer copy(const void* data, uint32_t in_size)
		{
			Buffer buffer;
			buffer.allocate(in_size);
			memcpy(buffer.data, data, in_size);
			return buffer;
		}

		void allocate(uint32_t in_size)
		{
			delete[](byte*) this->data;
			this->data = nullptr;

			if (in_size == 0)
				return;

			this->data = new byte[in_size];
			this->size = in_size;
		}

		void release()
		{
			delete[](byte*) this->data;
			this->data = nullptr;
			this->size = 0;
		}

		void zero_initialise()
		{
			if (this->data)
				memset(this->data, 0, this->size);
		}

		template <typename T> T& read(uint32_t offset = 0) { return *(T*)((byte*)this->data + offset); }

		byte* read_bytes(uint32_t in_size, uint32_t offset) const
		{
			CORE_ASSERT_BOOL(offset + in_size <= this->size);
			byte* buffer = new byte[in_size];
			memcpy(buffer, (byte*)this->data + offset, in_size);
			return buffer;
		}

		void write(const void* in_data, uint32_t in_size, uint32_t offset = 0) const
		{
			CORE_ASSERT_BOOL(offset + in_size <= this->size);
			memcpy((byte*)this->data + offset, in_data, in_size);
		}

		operator bool() const { return this->data; }

		byte& operator[](int index) { return ((byte*)this->data)[index]; }
		byte operator[](int index) const { return ((byte*)this->data)[index]; }

		template <typename T> T* as() const { return (T*)this->data; }

		[[nodiscard]] inline uint32_t get_size() const { return this->size; }

	public:
		void* data;
		uint32_t size;
	};

	struct BufferSafe : public Buffer {
		~BufferSafe() { release(); }

		static BufferSafe copy(const void* data, uint32_t in_size)
		{
			BufferSafe buffer;
			buffer.allocate(in_size);
			memcpy(buffer.data, data, in_size);
			return buffer;
		}
	};
} // namespace ForgottenEngine