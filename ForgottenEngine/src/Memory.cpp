#include "fg_pch.hpp"

#include "Memory.h"

#include <iostream>
#include <map>
#include <memory>
#include <mutex>

namespace ForgottenEngine {

static ForgottenEngine::AllocationStats global_stats;

static bool s_InInit = false;

void Allocator::init()
{
	if (allocator_data)
		return;

	s_InInit = true;
	AllocatorData* data = (AllocatorData*)Allocator::allocate_raw(sizeof(AllocatorData));
	new (data) AllocatorData();
	allocator_data = data;
	s_InInit = false;
}

void* Allocator::allocate_raw(size_t size) { return malloc(size); }

void* Allocator::allocate(size_t size)
{
	if (s_InInit)
		return allocate_raw(size);

	if (!allocator_data)
		init();

	void* memory = malloc(size);

	{
		std::scoped_lock<std::mutex> lock(allocator_data->mutex);
		Allocation& alloc = allocator_data->m_AllocationMap[memory];
		alloc.Memory = memory;
		alloc.Size = size;

		global_stats.TotalAllocated += size;
	}

	return memory;
}

void* Allocator::allocate(size_t size, const char* desc)
{
	if (!allocator_data)
		init();

	void* memory = malloc(size);

	{
		std::scoped_lock<std::mutex> lock(allocator_data->mutex);
		Allocation& alloc = allocator_data->m_AllocationMap[memory];
		alloc.Memory = memory;
		alloc.Size = size;
		alloc.Category = desc;

		global_stats.TotalAllocated += size;
		if (desc)
			allocator_data->allocation_stats_map[desc].TotalAllocated += size;
	}

	return memory;
}

void* Allocator::allocate(size_t size, const char* file, int line)
{
	if (!allocator_data)
		init();

	void* memory = malloc(size);

	{
		std::scoped_lock<std::mutex> lock(allocator_data->mutex);
		Allocation& alloc = allocator_data->m_AllocationMap[memory];
		alloc.Memory = memory;
		alloc.Size = size;
		alloc.Category = file;

		global_stats.TotalAllocated += size;
		allocator_data->allocation_stats_map[file].TotalAllocated += size;
	}

	return memory;
}

void Allocator::free(void* memory)
{
	if (memory == nullptr)
		return;

	{
		std::scoped_lock<std::mutex> lock(allocator_data->mutex);
		if (allocator_data->m_AllocationMap.find(memory) != allocator_data->m_AllocationMap.end()) {
			const Allocation& alloc = allocator_data->m_AllocationMap.at(memory);
			global_stats.TotalFreed += alloc.Size;
			if (alloc.Category)
				allocator_data->allocation_stats_map[alloc.Category].TotalFreed += alloc.Size;

			allocator_data->m_AllocationMap.erase(memory);
		} else {
			CORE_WARN("Memory block {0} not present in alloc map", memory);
		}
	}

	free(memory);
}

namespace Memory {
	const AllocationStats& get_allocation_stats() { return global_stats; }
}

}

#if FORGOTTEN_TRACK_MEMORY

void* operator new(size_t size) { return ForgottenEngine::Allocator::allocate(size); }

void* operator new[](size_t size) { return ForgottenEngine::Allocator::allocate(size); }

void* operator new(size_t size, const char* desc) { return ForgottenEngine::Allocator::allocate(size, desc); }

void* operator new[](size_t size, const char* desc) { return ForgottenEngine::Allocator::allocate(size, desc); }

void* operator new(size_t size, const char* file, int line)
{
	return ForgottenEngine::Allocator::allocate(size, file, line);
}

void* operator new[](size_t size, const char* file, int line)
{
	return ForgottenEngine::Allocator::allocate(size, file, line);
}

#endif
