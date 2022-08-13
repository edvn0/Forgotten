#pragma once

#include "render/Material.hpp"
#include "vulkan/VulkanShader.hpp"

namespace ForgottenEngine {

	class VulkanMaterial : public Material {
	public:
		VulkanMaterial(const Reference<Shader>& shader, std::string name);
		VulkanMaterial(Reference<Material> material, const std::string& name);
		~VulkanMaterial() override;

		void invalidate() override;
		void on_shader_reloaded() override;

		void set(const std::string& name, float value) override;
		void set(const std::string& name, int value) override;
		void set(const std::string& name, uint32_t value) override;
		void set(const std::string& name, bool value) override;
		void set(const std::string& name, const glm::ivec2& value) override;
		void set(const std::string& name, const glm::ivec3& value) override;
		void set(const std::string& name, const glm::ivec4& value) override;
		void set(const std::string& name, const glm::vec2& value) override;
		void set(const std::string& name, const glm::vec3& value) override;
		void set(const std::string& name, const glm::vec4& value) override;
		void set(const std::string& name, const glm::mat3& value) override;
		void set(const std::string& name, const glm::mat4& value) override;

		void set(const std::string& name, const Reference<Texture2D>& texture) override;
		void set(const std::string& name, const Reference<Texture2D>& texture, uint32_t arrayIndex) override;
		void set(const std::string& name, const Reference<TextureCube>& texture) override;
		void set(const std::string& name, const Reference<Image2D>& image) override;

		float& get_float(const std::string& name) override;
		int32_t& get_int(const std::string& name) override;
		uint32_t& get_uint(const std::string& name) override;
		bool& get_bool(const std::string& name) override;
		glm::vec2& get_vector2(const std::string& name) override;
		glm::vec3& get_vector3(const std::string& name) override;
		glm::vec4& get_vector4(const std::string& name) override;
		glm::mat3& get_matrix3(const std::string& name) override;
		glm::mat4& get_matrix4(const std::string& name) override;

		Reference<Texture2D> get_texture_2d(const std::string& name) override;
		Reference<TextureCube> get_texture_cube(const std::string& name) override;

		Reference<Texture2D> try_get_texture_2d(const std::string& name) override;
		Reference<TextureCube> try_get_texture_cube(const std::string& name) override;

		template <typename T> void set(const std::string& name, const T& value)
		{
			auto decl = find_uniform_declaration(name);
			CORE_ASSERT(decl, "Could not find uniform!");
			if (!decl)
				return;

			auto& buffer = uniform_storage_buffer;
			buffer.write((byte*)&value, decl->get_size(), decl->get_offset());
		}

		template <typename T> T& get(const std::string& name)
		{
			auto decl = find_uniform_declaration(name);
			CORE_ASSERT(decl, "Could not find uniform with name 'x'");
			auto& buffer = uniform_storage_buffer;
			return buffer.read<T>(decl->get_offset());
		}

		template <typename T> Reference<T> get_resource(const std::string& name)
		{
			auto decl = find_resource_declaration(name);
			CORE_ASSERT(decl, "Could not find uniform with name 'x'");
			uint32_t slot = decl->get_register();
			CORE_ASSERT(slot < material_textures.size(), "Texture slot is invalid!");
			return Reference<T>(material_textures[slot]);
		}

		template <typename T> Reference<T> try_get_resource(const std::string& name)
		{
			auto decl = find_resource_declaration(name);
			if (!decl)
				return nullptr;

			uint32_t slot = decl->get_register();
			if (slot >= material_textures.size())
				return nullptr;

			return Reference<T>(material_textures[slot]);
		}

		uint32_t get_flags() const override { return material_flags; }
		void set_flags(uint32_t flags) override { material_flags = flags; }
		bool get_flag(MaterialFlag flag) const override { return (uint32_t)flag & material_flags; }
		void set_flag(MaterialFlag flag, bool value = true) override
		{
			if (value) {
				material_flags |= (uint32_t)flag;
			} else {
				material_flags &= ~(uint32_t)flag;
			}
		}

		Reference<Shader> get_shader() override { return material_shader; }
		const std::string& get_name() const override { return material_name; }

		Buffer get_uniform_storage_buffer() { return uniform_storage_buffer; }

		void rt_update_for_rendering(
			const std::vector<std::vector<VkWriteDescriptorSet>>& uniformBufferWriteDescriptors = std::vector<std::vector<VkWriteDescriptorSet>>());
		void invalidate_descriptor_sets();

		VkDescriptorSet get_descriptor_set(uint32_t index) const
		{
			return !descriptor_sets[index].descriptor_sets.empty() ? descriptor_sets[index].descriptor_sets[0] : nullptr;
		}

	private:
		void init();
		void allocate_storage();

		void set_vulkan_descriptor(const std::string& name, const Reference<Texture2D>& texture);
		void set_vulkan_descriptor(const std::string& name, const Reference<Texture2D>& texture, uint32_t arrayIndex);
		void set_vulkan_descriptor(const std::string& name, const Reference<TextureCube>& texture);
		void set_vulkan_descriptor(const std::string& name, const Reference<Image2D>& image);

		const ShaderUniform* find_uniform_declaration(const std::string& name);
		const ShaderResourceDeclaration* find_resource_declaration(const std::string& name);

	private:
		Reference<Shader> material_shader;
		std::string material_name;

		enum class PendingDescriptorType { None = 0, Texture2D, TextureCube, Image2D };
		struct PendingDescriptor {
			PendingDescriptorType type = PendingDescriptorType::None;
			VkWriteDescriptorSet wds;
			VkDescriptorImageInfo image_info;
			Reference<Texture> texture;
			Reference<Image> image;
			VkDescriptorImageInfo submitted_image_info {};
		};

		struct PendingDescriptorArray {
			PendingDescriptorType type = PendingDescriptorType::None;
			VkWriteDescriptorSet wds;
			std::vector<VkDescriptorImageInfo> image_infos;
			std::vector<Reference<Texture>> textures;
			std::vector<Reference<Image>> images;
			VkDescriptorImageInfo submitted_image_info {};
		};
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptor>> resident_descriptors;
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptorArray>> resident_descriptors_array;
		std::vector<std::shared_ptr<PendingDescriptor>> pending_descriptors; // TODO: weak ref

		uint32_t material_flags = 0;

		Buffer uniform_storage_buffer;

		std::vector<Reference<Texture>> material_textures;
		std::vector<std::vector<Reference<Texture>>> texture_arrays;
		std::vector<Reference<Image>> images;

		VulkanShader::ShaderMaterialDescriptorSet descriptor_sets[3];

		std::vector<std::vector<VkWriteDescriptorSet>> write_descriptors;
		std::vector<bool> dirty_descriptor_sets;
	};

} // namespace ForgottenEngine
