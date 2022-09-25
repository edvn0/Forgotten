#include "fg_pch.hpp"

#include "render/Material.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanMaterial.hpp"

namespace ForgottenEngine {

	Reference<Material> Material::create(const Reference<Shader>& shader, const std::string& name)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanMaterial>::create(shader, name);
		}
		core_assert(false, "Unknown RendererAPI");
	}

	Reference<Material> Material::copy(const Reference<Material>& other, const std::string& name)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanMaterial>::create(other, name);
		}
		core_assert(false, "Unknown RendererAPI");
	}

} // namespace ForgottenEngine
