#include "fg_pch.hpp"

#include "render/RenderCommandQueue.hpp"

#define HZ_RENDER_TRACE(...) HZ_CORE_TRACE(__VA_ARGS__)

namespace ForgottenEngine {

static constexpr auto COMMAND_QUEUE_SIZE = 10 * 1024 * 1024;

RenderCommandQueue::RenderCommandQueue()
{
	command_buffer = new uint8_t[COMMAND_QUEUE_SIZE]; // 10mb buffer
	command_buffer_ptr = command_buffer;
	memset(command_buffer, 0, COMMAND_QUEUE_SIZE);
}

RenderCommandQueue::~RenderCommandQueue() { delete[] command_buffer; }

void* RenderCommandQueue::allocate(RenderCommandFunction fn, uint32_t size)
{
	// TODO: alignment
	*(RenderCommandFunction*)command_buffer_ptr = fn;
	command_buffer_ptr += sizeof(RenderCommandFunction);

	*(uint32_t*)command_buffer_ptr = size;
	command_buffer_ptr += sizeof(uint32_t);

	void* memory = command_buffer_ptr;
	command_buffer_ptr += size;

	command_count++;
	return memory;
}

void RenderCommandQueue::execute()
{
	// HZ_RENDER_TRACE("RenderCommandQueue::Execute -- {0} commands, {1} bytes", command_count,
	// (command_buffer_ptr - command_buffer));

	auto* buffer = command_buffer;

	for (uint32_t i = 0; i < command_count; i++) {
		RenderCommandFunction function = *(RenderCommandFunction*)buffer;
		buffer += sizeof(RenderCommandFunction);

		uint32_t size = *(uint32_t*)buffer;
		buffer += sizeof(uint32_t);
		function(buffer);
		buffer += size;
	}

	command_buffer_ptr = command_buffer;
	command_count = 0;
}

}
