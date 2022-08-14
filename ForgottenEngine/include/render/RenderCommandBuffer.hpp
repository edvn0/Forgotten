//
//  RenderCommandBuffer.hpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-25.
//

#pragma once

#include "Pipeline.hpp"
#include "Reference.hpp"

namespace ForgottenEngine {

	class RenderCommandBuffer : public ReferenceCounted {
	public:
		virtual ~RenderCommandBuffer() = default;

		virtual void begin() = 0;
		virtual void end() = 0;
		virtual void submit() = 0;
		static Reference<RenderCommandBuffer> create(uint32_t count = 0);
		static Reference<RenderCommandBuffer> create_from_swapchain();
	};

} // namespace ForgottenEngine
