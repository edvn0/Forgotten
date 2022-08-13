#include "fg_pch.hpp"

#include "vulkan/VulkanAssetLibrary.hpp"

namespace ForgottenEngine {

	const std::shared_ptr<RenderMaterial>& VulkanAssetLibrary::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
	{
		RenderMaterial mat;
		mat.pipeline = pipeline;
		mat.layout = layout;
		materials[name] = std::make_shared<RenderMaterial>(mat);
		return materials[name];
	}

	const std::shared_ptr<RenderMaterial>& VulkanAssetLibrary::material(const std::string& name)
	{
		// search for the object, and return nullptr if not found
		auto it = materials.find(name);
		if (it == materials.end()) {
			CORE_ERROR("Could not find material with name [{}].", name);
		} else {
			return (*it).second;
		}
	}

	const std::shared_ptr<Mesh>& VulkanAssetLibrary::mesh(const std::string& name)
	{
		auto it = meshes.find(name);
		if (it == meshes.end()) {
			CORE_ERROR("Could not find mesh with name [{}].", name);
		} else {
			return (*it).second;
		}
	}

	const std::shared_ptr<Image>& VulkanAssetLibrary::texture(const std::string& name)
	{
		auto it = textures.find(name);
		if (it == textures.end()) {
			CORE_ERROR("Could not find texture with name [{}].", name);
		} else {
			return (*it).second;
		}
	}
} // namespace ForgottenEngine