#pragma once

#include "Common.hpp"

#include "Mesh.hpp"

#include "VulkanImage.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

using Texture = VulkanImage;

struct RenderMaterial {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct VulkanRenderObject {
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<RenderMaterial> material;
	glm::mat4 transform{};
};

class VulkanAssetLibrary {
private:
	std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes{};
	std::unordered_map<std::string, std::shared_ptr<RenderMaterial>> materials{};
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures{};

public:
	VulkanAssetLibrary() = default;

	const std::shared_ptr<RenderMaterial>& create_material(
		VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	void add_mesh(std::string name, std::shared_ptr<Mesh>&& new_mesh) { meshes[name] = std::move(new_mesh); }
	void add_material(std::string name, RenderMaterial&& new_material)
	{
		materials[name] = std::make_shared<RenderMaterial>(new_material);
	}
	void add_texture(std::string name, std::shared_ptr<Texture>&& texture) { textures[name] = std::move(texture); }

	const std::shared_ptr<Mesh>& mesh(const std::string& name);
	const std::shared_ptr<RenderMaterial>& material(const std::string& name);
	const std::shared_ptr<Texture>& texture(const std::string& name);
};
}
