#pragma once

#include <unordered_map>
#include <vector>

namespace ForgottenEngine {

	template <typename T> class SBuffer {
	public:
	private:
		T* buffer = nullptr;
	};

	template <typename T, bool SerializeSize = true> class SArray {
	public:
	private:
		std::vector<T> array;
	};

	template <typename Key, typename Value, bool SerializeSize = true> class SMap {
	public:
	private:
		std::unordered_map<Key, Value> map;
	};

} // namespace ForgottenEngine
