#pragma once

#include "render/Material.hpp"

namespace ForgottenEngine {

class VulkanMaterial : public Material {
public:
	explicit VulkanMaterial(const Reference<Shader>& shader, const std::string& name = "");
	void invalidate() override;
	void on_shader_reloaded() override;

	void set(const std::string& name, float value) override;
	void set(const std::string& name, int value) override;
	void set(const std::string& name, uint32_t value) override;
	void set(const std::string& name, bool value) override;
	void set(const std::string& name, const glm::vec2& value) override;
	void set(const std::string& name, const glm::vec3& value) override;
	void set(const std::string& name, const glm::vec4& value) override;
	void set(const std::string& name, const glm::ivec2& value) override;
	void set(const std::string& name, const glm::ivec3& value) override;
	void set(const std::string& name, const glm::ivec4& value) override;
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
	Reference<TextureCube> try_get_texture_cube(const std::string& name) override
	{
		return Reference<TextureCube>();
	}

	uint32_t get_flags() const override;
	void set_flags(uint32_t flags) override;
	bool get_flag(MaterialFlag flag) const override;
	void set_flag(MaterialFlag flag, bool value) override;
	Reference<Shader> get_shader() override;
	const std::string& get_name() const override;

	VkDescriptorSet get_descriptor_set(uint32_t buffer_index) { return descriptor_sets[buffer_index]; };
	Buffer get_uniform_storage_buffer() { return buffer; };

	void rt_update_for_rendering(const std::vector<std::vector<VkWriteDescriptorSet>>& write_descriptors){};
	void rt_update_for_rendering() { }

private:
	std::vector<VkDescriptorSet> descriptor_sets;
	Buffer buffer;
};
}