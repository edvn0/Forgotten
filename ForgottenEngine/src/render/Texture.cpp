//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/Texture.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanTexture.hpp"

namespace ForgottenEngine {

	Reference<Texture2D> Texture2D::create(ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanTexture2D>::create(format, width, height, data, properties);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

	Reference<Texture2D> Texture2D::create(const std::string& path, TextureProperties properties)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanTexture2D>::create(path, properties);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

	Reference<TextureCube> TextureCube::create(ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanTextureCube>::create(format, width, height, data, properties);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

	Reference<TextureCube> TextureCube::create(const std::string& path, TextureProperties properties)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanTextureCube>::create(path, properties);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine