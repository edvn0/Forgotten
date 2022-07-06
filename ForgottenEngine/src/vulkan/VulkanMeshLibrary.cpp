#include "fg_pch.hpp"

#include "vulkan/VulkanMeshLibrary.hpp"

namespace ForgottenEngine {

const std::shared_ptr<RenderMaterial>& VulkanMeshLibrary::create_material(
	VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	RenderMaterial mat;
	mat.pipeline = pipeline;
	mat.layout = layout;
	materials[name] = std::make_unique<RenderMaterial>(mat);
	return materials[name];
}

const std::shared_ptr<RenderMaterial>& VulkanMeshLibrary::material(const std::string& name)
{
	// search for the object, and return nullptr if not found
	auto it = materials.find(name);
	if (it == materials.end()) {
		CORE_ERROR("Could not find material with name [{}].", name);
	} else {
		return (*it).second;
	}
}

const std::shared_ptr<Mesh>& VulkanMeshLibrary::mesh(const std::string& name)
{
	auto it = meshes.find(name);
	if (it == meshes.end()) {
		CORE_ERROR("Could not find mesh with name [{}].", name);
	} else {
		return (*it).second;
	}
}
}