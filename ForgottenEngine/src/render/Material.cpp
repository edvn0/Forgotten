#include "fg_pch.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "render/Material.hpp"
#include "vulkan/VulkanMaterial.hpp"

namespace ForgottenEngine {

Reference<Material> Material::create(const ShaderPair& shader, const std::string& name)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanMaterial>::create(shader, name);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

Reference<Material> Material::copy(const Reference<Material>& other, const std::string& name)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanMaterial>::create(other, name);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

} // namespace ForgottenEngine
