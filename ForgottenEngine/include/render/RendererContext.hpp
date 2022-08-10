#pragma once

#include "Reference.hpp"

struct GLFWwindow;

namespace ForgottenEngine {

	class RendererContext : public ReferenceCounted {
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void init() = 0;

		static Reference<RendererContext> create();
	};

} // namespace ForgottenEngine
