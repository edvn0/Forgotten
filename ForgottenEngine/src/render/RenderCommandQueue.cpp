#include "fg_pch.hpp"

#include "render/RenderCommandQueue.hpp"

namespace ForgottenEngine {

	static constexpr auto COMMAND_QUEUE_SIZE = 10 * 1024 * 1024;

	RenderCommandQueue::RenderCommandQueue()
	{
		command_buffer = hnew uint8_t[COMMAND_QUEUE_SIZE]; // 10mb buffer
		command_buffer_ptr = command_buffer;
		memset(command_buffer, 0, COMMAND_QUEUE_SIZE);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] command_buffer;
	}

	void* RenderCommandQueue::allocate(RenderCommandFn fn, uint32_t size)
	{
		// TODO: alignment
		*(RenderCommandFn*)command_buffer_ptr = fn;
		command_buffer_ptr += sizeof(RenderCommandFn);

		*(uint32_t*)command_buffer_ptr = size;
		command_buffer_ptr += sizeof(uint32_t);

		void* memory = command_buffer_ptr;
		command_buffer_ptr += size;

		command_count++;
		return memory;
	}

	void RenderCommandQueue::execute()
	{
		byte* buffer = command_buffer;

		for (uint32_t i = 0; i < command_count; i++) {
			RenderCommandFn function = *(RenderCommandFn*)buffer;
			buffer += sizeof(RenderCommandFn);

			uint32_t size = *(uint32_t*)buffer;
			buffer += sizeof(uint32_t);
			function(buffer);
			buffer += size;
		}

		command_buffer_ptr = command_buffer;
		command_count = 0;
	}

} // namespace ForgottenEngine
