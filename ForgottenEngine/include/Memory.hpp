//
// Created by Edwin Carlsson on 2022-08-04.
//

#pragma once

#include <map>
#include <mutex>

namespace ForgottenEngine {

	struct AllocationStats {
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	struct Allocation {
		void* Memory = nullptr;
		size_t Size = 0;
		const char* Category = nullptr;
	};

	namespace Memory {
		const AllocationStats& get_allocation_stats();
	}

	template <class T>
	struct Mallocator {
		typedef T value_type;

		Mallocator() = default;
		template <class U>
		constexpr explicit Mallocator(const Mallocator<U>&) noexcept { }

		T* allocate(std::size_t n)
		{
#undef max
			if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
				return p;
			}

			throw std::bad_alloc();
		}

		void deallocate(T* p, std::size_t n) noexcept { std::free(p); }
	};

	struct AllocatorData {
		using MapAlloc = Mallocator<std::pair<const void* const, Allocation>>;
		using StatsMapAlloc = Mallocator<std::pair<const char* const, AllocationStats>>;

		using AllocationStatsMap = std::map<const char*, AllocationStats, std::less<const char*>, StatsMapAlloc>;

		std::map<const void*, Allocation, std::less<const void*>, MapAlloc> m_AllocationMap;
		AllocationStatsMap allocation_stats_map;

		std::mutex mutex, stats_mutex;
	};

	class Allocator {
	public:
		static void init();

		static void* allocate_raw(size_t size);

		static void* allocate(size_t size);
		static void* allocate(size_t size, const char* desc);
		static void* allocate(size_t size, const char* file, int line);
		static void free(void* memory);

		static const AllocatorData::AllocationStatsMap& GetAllocationStats()
		{
			return allocator_data->allocation_stats_map;
		}

	private:
		inline static AllocatorData* allocator_data = nullptr;
	};

} // namespace ForgottenEngine

#if FORGOTTEN_TRACE_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* desc);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* desc);
void __CRTDECL operator delete[](void* memory, const char* file, int line);

#define hnew new (__FILE__, __LINE__)
#define hdelete delete

#else

#define hnew new
#define hdelete delete

#endif
