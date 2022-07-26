#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

enum class RendererAPIType { None, Vulkan };

enum class PrimitiveType { None = 0, Triangles, Lines };

class RendererAPI {
public:
	virtual ~RendererAPI(){};

	virtual void init() = 0;
	virtual void shut_down() = 0;

	virtual void begin_frame() = 0;
	virtual void end_frame() = 0;
	static RendererAPIType current() { return current_api; }
	static void set_api(RendererAPIType api)
	{
		CORE_ASSERT(api == RendererAPIType::Vulkan, "Must be Vulkan for now.");
		current_api = api;
	}

private:
	inline static RendererAPIType current_api = RendererAPIType::Vulkan;
};

} // namespace ForgottenEngine
