//
// Created by Edwin Carlsson on 2022-08-05.
//

#include "fg_pch.hpp"

#include "render/Image.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanImage.hpp"

namespace ForgottenEngine {

	Reference<Image2D> Image2D::create(ImageSpecification specification, Buffer buffer)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanImage2D>::create(specification);
		}
		core_assert(false, "Unknown RendererAPI");
	}

	Reference<Image2D> Image2D::create(ImageSpecification specification, const void* data)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanImage2D>::create(specification);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine