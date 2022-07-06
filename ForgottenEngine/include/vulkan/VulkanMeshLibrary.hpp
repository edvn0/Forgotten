#pragma once

#include <vulkan/vulkan.h>

#include "Common.hpp"
#include "Mesh.hpp"

namespace ForgottenEngine {

struct RenderMaterial {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct VulkanRenderObject {
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<RenderMaterial> material;
	glm::mat4 transform{};
};

class VulkanMeshLibrary {
private:
	std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes{};
	std::unordered_map<std::string, std::shared_ptr<RenderMaterial>> materials{};

public:
	VulkanMeshLibrary() = default;

	const std::shared_ptr<RenderMaterial>& create_material(
		VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	void add_mesh(std::string name, std::shared_ptr<Mesh>&& new_mesh) { meshes[name] = std::move(new_mesh); }
	void add_material(std::string name, RenderMaterial&& new_material)
	{
		materials[name] = std::make_unique<RenderMaterial>(new_material);
	}

	const std::shared_ptr<Mesh>& mesh(const std::string& name);
	const std::shared_ptr<RenderMaterial>& material(const std::string& name);
};
}
