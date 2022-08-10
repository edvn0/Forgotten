#pragma once

#include "Reference.hpp"
#include "render/RenderPass.hpp"
#include "render/Shader.hpp"
#include "render/UniformBuffer.hpp"
#include "render/VertexBuffer.hpp"

namespace ForgottenEngine {

	enum class PrimitiveTopology { None = 0,
		Points,
		Lines,
		Triangles,
		LineStrip,
		TriangleStrip,
		TriangleFan };

	enum class DepthCompareOperator {
		None = 0,
		Never,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,
		Equal,
		Always,
	};

	struct PipelineSpecification {
		Reference<Shader> Shader;
		VertexBufferLayout Layout;
		VertexBufferLayout InstanceLayout;
		Reference<RenderPass> RenderPass;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		DepthCompareOperator DepthOperator = DepthCompareOperator::GreaterOrEqual;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool Wireframe = false;
		float LineWidth = 1.0f;

		std::string DebugName;
	};

	struct PipelineStatistics {
		uint64_t InputAssemblyVertices = 0;
		uint64_t InputAssemblyPrimitives = 0;
		uint64_t VertexShaderInvocations = 0;
		uint64_t ClippingInvocations = 0;
		uint64_t ClippingPrimitives = 0;
		uint64_t FragmentShaderInvocations = 0;
		uint64_t ComputeShaderInvocations = 0;
	};

	class Pipeline : public ReferenceCounted {
	public:
		virtual ~Pipeline() = default;

		virtual PipelineSpecification& get_specification() = 0;
		virtual const PipelineSpecification& get_specification() const = 0;

		virtual void invalidate() = 0;

		// TEMP: remove this when render command buffers are a thing
		virtual void bind() = 0;

		virtual void set_uniform_buffer(Reference<UniformBuffer> ub, uint32_t binding, uint32_t set) = 0;

		static Reference<Pipeline> create(const PipelineSpecification& spec);
	};

} // namespace ForgottenEngine
