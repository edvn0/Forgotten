//
// Created by Edwin Carlsson on 2022-08-10.
//

#pragma once

#include "Common.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "render/Shader.hpp"

namespace ForgottenEngine {

	class ComputePipeline : public ReferenceCounted {
	public:
		virtual ~ComputePipeline() = default;
		virtual void begin(Reference<RenderCommandBuffer> command_buffer) = 0;
		virtual void end() = 0;

		virtual Reference<Shader> get_shader() = 0;

		static Reference<ComputePipeline> create(Reference<Shader> computeShader);
	};

} // namespace ForgottenEngine
