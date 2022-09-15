#pragma once

#include "Buffer.hpp"

#include <map>
#include <unordered_map>

namespace ForgottenEngine {

	class StreamWriter {
	public:
		virtual ~StreamWriter() = default;

		virtual bool is_stream_good() const = 0;
		virtual uint64_t get_stream_position() = 0;
		virtual void set_stream_position(uint64_t position) = 0;
		virtual bool write_data(const char* data, size_t size) = 0;

		operator bool() const { return is_stream_good(); }

		void write_buffer(Buffer buffer, bool writeSize = true);
		void write_zero(uint64_t size);
		void write_string(const std::string& string);

		template <typename T> void write_raw(const T& type)
		{
			bool success = write_data((char*)&type, sizeof(T));
			CORE_ASSERT_BOOL(success);
		}

		template <typename T> void write_object(const T& obj) { T::serialize(this, obj); }

		template <typename Key, typename Value> void write_map(const std::unordered_map<Key, Value>& map, bool writeSize = true)
		{
			if (writeSize)
				write_raw<uint32_t>((uint32_t)map.size());

			for (const auto& [key, value] : map) {
				if constexpr (std::is_trivial<Key>())
					write_raw<Key>(key);
				else
					write_object<Key>(key);

				if constexpr (std::is_trivial<Value>())
					write_raw<Value>(value);
				else
					write_object<Value>(value);
			}
		}

		template <typename Key, typename Value> void write_map(const std::map<Key, Value>& map, bool writeSize = true)
		{
			if (writeSize)
				write_raw<uint32_t>((uint32_t)map.size());

			for (const auto& [key, value] : map) {
				if constexpr (std::is_trivial<Key>())
					write_raw<Key>(key);
				else
					write_object<Key>(key);

				if constexpr (std::is_trivial<Value>())
					write_raw<Value>(value);
				else
					write_object<Value>(value);
			}
		}

		template <typename Value> void write_map(const std::unordered_map<std::string, Value>& map, bool writeSize = true)
		{
			if (writeSize)
				write_raw<uint32_t>((uint32_t)map.size());

			for (const auto& [key, value] : map) {
				write_string(key);

				if constexpr (std::is_trivial<Value>())
					write_raw<Value>(value);
				else
					write_object<Value>(value);
			}
		}

		template <typename T> void write_array(const std::vector<T>& array, bool writeSize = true)
		{
			if (writeSize)
				write_raw<uint32_t>((uint32_t)array.size());

			for (const auto& element : array) {
				if constexpr (std::is_trivial<T>())
					write_raw<T>(element);
				else
					write_object<T>(element);
			}
		}

		template <> void write_array(const std::vector<std::string>& array, bool writeSize)
		{
			if (writeSize)
				write_raw<uint32_t>((uint32_t)array.size());

			for (const auto& element : array)
				write_string(element);
		}
	};

} // namespace ForgottenEngine
