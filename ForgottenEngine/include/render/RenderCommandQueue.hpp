#pragma once

#include <cstdint>

namespace ForgottenEngine {

class RenderCommandQueue {
public:
	typedef void (*RenderCommandFunction)(void*);

	RenderCommandQueue();
	~RenderCommandQueue();

	void* allocate(RenderCommandFunction func, uint32_t size);

	void execute();

private:
	uint8_t* command_buffer;
	uint8_t* command_buffer_ptr;
	uint32_t command_count = 0;
};

}
