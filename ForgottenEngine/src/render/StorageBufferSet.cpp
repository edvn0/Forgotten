//
// Created by Edwin Carlsson on 2022-08-03.
//

#include "fg_pch.hpp"

#include "render/StorageBufferSet.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanStorageBufferSet.hpp"

namespace ForgottenEngine {

	Reference<StorageBufferSet> StorageBufferSet::create(uint32_t size)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanStorageBufferSet>::create(size);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine