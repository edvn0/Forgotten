//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/Pipeline.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanPipeline.hpp"

namespace ForgottenEngine {

	Reference<Pipeline> Pipeline::create(const PipelineSpecification& spec)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan: {
			auto pipeline = Reference<VulkanPipeline>::create(spec);
			pipeline->invalidate();
			return pipeline;
		}
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine