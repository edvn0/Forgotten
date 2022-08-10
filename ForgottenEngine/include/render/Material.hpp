#pragma once

#include "Assets.hpp"
#include "Common.hpp"
#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <unordered_set>

namespace ForgottenEngine {

	enum class MaterialFlag {
		None = BIT(0),
		DepthTest = BIT(1),
		Blend = BIT(2),
		TwoSided = BIT(3),
		DisableShadowCasting = BIT(4)
	};

	class Material : public ReferenceCounted {
	public:
		virtual ~Material() = default;

		virtual void invalidate() = 0;
		virtual void on_shader_reloaded() = 0;

		virtual void set(const std::string& name, float value) = 0;
		virtual void set(const std::string& name, int value) = 0;
		virtual void set(const std::string& name, uint32_t value) = 0;
		virtual void set(const std::string& name, bool value) = 0;
		virtual void set(const std::string& name, const glm::vec2& value) = 0;
		virtual void set(const std::string& name, const glm::vec3& value) = 0;
		virtual void set(const std::string& name, const glm::vec4& value) = 0;
		virtual void set(const std::string& name, const glm::ivec2& value) = 0;
		virtual void set(const std::string& name, const glm::ivec3& value) = 0;
		virtual void set(const std::string& name, const glm::ivec4& value) = 0;

		virtual void set(const std::string& name, const glm::mat3& value) = 0;
		virtual void set(const std::string& name, const glm::mat4& value) = 0;

		virtual void set(const std::string& name, const Reference<Texture2D>& texture) = 0;
		virtual void set(const std::string& name, const Reference<Texture2D>& texture, uint32_t arrayIndex) = 0;
		virtual void set(const std::string& name, const Reference<TextureCube>& texture) = 0;
		virtual void set(const std::string& name, const Reference<Image2D>& image) = 0;

		virtual float& get_float(const std::string& name) = 0;
		virtual int32_t& get_int(const std::string& name) = 0;
		virtual uint32_t& get_uint(const std::string& name) = 0;
		virtual bool& get_bool(const std::string& name) = 0;
		virtual glm::vec2& get_vector2(const std::string& name) = 0;
		virtual glm::vec3& get_vector3(const std::string& name) = 0;
		virtual glm::vec4& get_vector4(const std::string& name) = 0;
		virtual glm::mat3& get_matrix3(const std::string& name) = 0;
		virtual glm::mat4& get_matrix4(const std::string& name) = 0;

		virtual Reference<Texture2D> get_texture_2d(const std::string& name) = 0;
		virtual Reference<TextureCube> get_texture_cube(const std::string& name) = 0;

		virtual Reference<Texture2D> try_get_texture_2d(const std::string& name) = 0;
		virtual Reference<TextureCube> try_get_texture_cube(const std::string& name) = 0;

		virtual uint32_t get_flags() const = 0;
		virtual void set_flags(uint32_t flags) = 0;

		virtual bool get_flag(MaterialFlag flag) const = 0;
		virtual void set_flag(MaterialFlag flag, bool value = true) = 0;

		virtual Reference<Shader> get_shader() = 0;
		virtual const std::string& get_name() const = 0;

		static Reference<Material> create(const Reference<Shader>& shader, const std::string& name = "");
		static Reference<Material> copy(const Reference<Material>& other, const std::string& name = "");
	};

} // namespace ForgottenEngine
