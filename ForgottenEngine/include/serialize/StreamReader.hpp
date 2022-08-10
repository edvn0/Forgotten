#pragma once

#include "Buffer.hpp"

namespace ForgottenEngine {

	class StreamReader {
	public:
		virtual ~StreamReader() = default;

		virtual bool is_stream_good() const = 0;
		virtual uint64_t get_stream_position() = 0;
		virtual void set_stream_position(uint64_t position) = 0;
		virtual bool read_data(char* destination, size_t size) = 0;

		operator bool() const { return is_stream_good(); }

		void read_buffer(Buffer& buffer, uint32_t size = 0);
		void read_string(std::string& string);

		template <typename T>
		void read_raw(T& type)
		{
			bool success = read_data((char*)&type, sizeof(T));
			CORE_ASSERT(success, "Could not read data into type T.");
		}

		template <typename T>
		void read_object(T& obj) { T::deserialize(this, obj); }

		template <typename Key, typename Value>
		void read_map(std::map<Key, Value>& map, uint32_t size = 0)
		{
			if (size == 0)
				read_raw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++) {
				Key key;
				if constexpr (std::is_trivial<Key>())
					read_raw<Key>(key);
				else
					read_object<Key>(key);

				if constexpr (std::is_trivial<Value>())
					read_raw<Value>(map[key]);
				else
					read_object<Value>(map[key]);
			}
		}

		template <typename Key, typename Value>
		void read_map(std::unordered_map<Key, Value>& map, uint32_t size = 0)
		{
			if (size == 0)
				read_raw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++) {
				Key key;
				if constexpr (std::is_trivial<Key>())
					read_raw<Key>(key);
				else
					read_object<Key>(key);

				if constexpr (std::is_trivial<Value>())
					read_raw<Value>(map[key]);
				else
					read_object<Value>(map[key]);
			}
		}

		template <typename Value>
		void read_map(std::unordered_map<std::string, Value>& map, uint32_t size = 0)
		{
			if (size == 0)
				read_raw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++) {
				std::string key;
				read_string(key);

				if constexpr (std::is_trivial<Value>())
					read_raw<Value>(map[key]);
				else
					read_object<Value>(map[key]);
			}
		}

		template <typename T>
		void read_array(std::vector<T>& array, uint32_t size = 0)
		{
			if (size == 0)
				read_raw<uint32_t>(size);

			array.resize(size);

			for (uint32_t i = 0; i < size; i++) {
				if constexpr (std::is_trivial<T>())
					read_raw<T>(array[i]);
				else
					read_object<T>(array[i]);
			}
		}

		template <>
		void read_array(std::vector<std::string>& array, uint32_t size)
		{
			if (size == 0)
				read_raw<uint32_t>(size);

			array.resize(size);

			for (uint32_t i = 0; i < size; i++)
				read_string(array[i]);
		}
	};

} // namespace ForgottenEngine
