//
// Created by Edwin Carlsson on 2022-08-10.
//

#include "fg_pch.hpp"

#include "render/ComputePipeline.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanComputePipeline.hpp"

namespace ForgottenEngine {

	Reference<ComputePipeline> ComputePipeline::create(Reference<Shader> computeShader)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanComputePipeline>::create(computeShader);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine