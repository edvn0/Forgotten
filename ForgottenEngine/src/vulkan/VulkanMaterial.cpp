//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanMaterial.hpp"

namespace ForgottenEngine {

VulkanMaterial::VulkanMaterial(const ShaderPair& shader, const std::string& name) { }

VulkanMaterial::VulkanMaterial(const Reference<Material>& other, const std::string& name) { }

void VulkanMaterial::invalidate() { }

void VulkanMaterial::on_shader_reloaded() { }

void VulkanMaterial::set(const std::string& name, float value){};

void VulkanMaterial::set(const std::string& name, int value){};

void VulkanMaterial::set(const std::string& name, uint32_t value){};

void VulkanMaterial::set(const std::string& name, bool value){};

void VulkanMaterial::set(const std::string& name, const glm::vec2& value){};

void VulkanMaterial::set(const std::string& name, const glm::vec3& value){};

void VulkanMaterial::set(const std::string& name, const glm::vec4& value){};

void VulkanMaterial::set(const std::string& name, const glm::ivec2& value){};

void VulkanMaterial::set(const std::string& name, const glm::ivec3& value){};

void VulkanMaterial::set(const std::string& name, const glm::ivec4& value){};

void VulkanMaterial::set(const std::string& name, const glm::mat3& value){};

void VulkanMaterial::set(const std::string& name, const glm::mat4& value){};

void VulkanMaterial::set(const std::string& name, const Reference<Texture2D>& texture){};

void VulkanMaterial::set(const std::string& name, const Reference<Texture2D>& texture, uint32_t arrayIndex){};

void VulkanMaterial::set(const std::string& name, const Reference<TextureCube>& texture){};

void VulkanMaterial::set(const std::string& name, const Reference<Image2D>& image){};

float& VulkanMaterial::get_float(const std::string& name) { }

int32_t& VulkanMaterial::get_int(const std::string& name) { }

uint32_t& VulkanMaterial::get_uint(const std::string& name) { }

bool& VulkanMaterial::get_bool(const std::string& name) { }

glm::vec2& VulkanMaterial::get_vector2(const std::string& name) { }

glm::vec3& VulkanMaterial::get_vector3(const std::string& name) { }

glm::vec4& VulkanMaterial::get_vector4(const std::string& name) { }

glm::mat3& VulkanMaterial::get_matrix3(const std::string& name) { }

glm::mat4& VulkanMaterial::get_matrix4(const std::string& name) { }

Reference<Texture2D> VulkanMaterial::get_texture_2d(const std::string& name) { }

Reference<TextureCube> VulkanMaterial::get_texture_cube(const std::string& name) { }

Reference<Texture2D> VulkanMaterial::try_get_texture_2d(const std::string& name) { }

Reference<TextureCube> VulkanMaterial::try_get_texture_cube(const std::string& name) { }

uint32_t VulkanMaterial::get_flags() const { }

void VulkanMaterial::set_flags(uint32_t flags) { }

bool VulkanMaterial::get_flag(MaterialFlag flag) const { }

void VulkanMaterial::set_flag(MaterialFlag flag, bool value) { }

Reference<Shader> VulkanMaterial::get_shader() { }

const std::string& VulkanMaterial::get_name() const { }

}