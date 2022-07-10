//
// Created by Edwin Carlsson on 2022-07-08.
//

#include "MemoryMapper.hpp"

namespace ForgottenEngine::MemoryMapper {

template <typename T, typename Func>
void effect_mmap(VmaAllocator& allocator, AllocatedBuffer& buffer, Func&& effect)
{
	void* object_data;
	vmaMapMemory(allocator, buffer.allocation, &object_data);
	auto* t_pointer = static_cast<T*>(object_data);
	effect(t_pointer);
	vmaUnmapMemory(allocator, buffer.allocation);
}

void effect_mmap(VmaAllocator& allocator, AllocatedBuffer& buffer, std::function<void(void*)>&& effect)
{
	void* object_data;
	vmaMapMemory(allocator, buffer.allocation, &object_data);
	effect(object_data);
	vmaUnmapMemory(allocator, buffer.allocation);
}

void effect_mmap(VmaAllocator& allocator, AllocatedBuffer& buffer, const std::function<void(void*)>& effect)
{
	void* object_data;
	vmaMapMemory(allocator, buffer.allocation, &object_data);
	effect(object_data);
	vmaUnmapMemory(allocator, buffer.allocation);
}

}