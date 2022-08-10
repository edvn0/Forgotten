#pragma once

#include "Common.hpp"
#include "VulkanImage.hpp"
#include "render/Mesh.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

	struct RenderMaterial {
		VkDescriptorSet texture_set { nullptr };
		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct VulkanRenderObject {
		std::shared_ptr<Mesh> mesh;
		std::shared_ptr<RenderMaterial> material;
		glm::mat4 transform {};
	};

	class VulkanAssetLibrary {
	private:
		std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes {};
		std::unordered_map<std::string, std::shared_ptr<RenderMaterial>> materials {};
		std::unordered_map<std::string, std::shared_ptr<Image>> textures {};

	public:
		VulkanAssetLibrary() = default;

		const std::shared_ptr<RenderMaterial>& create_material(
			VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

		void add_mesh(const std::string& name, std::shared_ptr<Mesh>&& new_mesh)
		{
			meshes[name] = std::move(new_mesh);
		}
		void add_material(const std::string& name, RenderMaterial&& new_material)
		{
			materials[name] = std::make_shared<RenderMaterial>(new_material);
		}
		void add_texture(const std::string& name, std::shared_ptr<Image>&& texture)
		{
			textures[name] = std::move(texture);
		}

		const std::shared_ptr<Mesh>& mesh(const std::string& name);
		const std::shared_ptr<RenderMaterial>& material(const std::string& name);
		const std::shared_ptr<Image>& texture(const std::string& name);
	};
} // namespace ForgottenEngine
