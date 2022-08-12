#pragma once

#include <cstdint>

namespace ForgottenEngine {

	class RenderCommandQueue {
	public:
		typedef void (*RenderCommandFn)(void*);

		RenderCommandQueue();
		~RenderCommandQueue();

		void* allocate(RenderCommandFn func, uint32_t size);

		void execute();

	private:
		uint8_t* command_buffer;
		uint8_t* command_buffer_ptr;
		uint32_t command_count = 0;
	};

} // namespace ForgottenEngine
