#pragma once

#include "Asset.hpp"
#include "render/Material.hpp"

#include <unordered_map>

namespace ForgottenEngine {

	class MaterialAsset : public Asset {
	public:
		MaterialAsset(bool transparent = false);
		MaterialAsset(Reference<Material> material);
		~MaterialAsset();

		glm::vec3& get_albedo_colour();
		void set_albedo_colour(const glm::vec3& color);

		float& get_metalness();
		void set_metalness(float value);

		float& get_roughness();
		void set_roughness(float value);

		float& get_emission();
		void set_emission(float value);

		// Textures
		Reference<Texture2D> get_albedo_map();
		void set_albedo_map(Reference<Texture2D> texture);
		void clear_albedo_map();

		Reference<Texture2D> get_normal_map();
		void set_normal_map(Reference<Texture2D> texture);
		bool is_using_normal_map();
		void set_use_normal_map(bool value);
		void clear_normal_map();

		Reference<Texture2D> get_metalness_map();
		void set_metalness_map(Reference<Texture2D> texture);
		void clear_metalness_map();

		Reference<Texture2D> get_roughness_map();
		void set_roughness_map(Reference<Texture2D> texture);
		void clear_roughness_map();

		float& get_transparency();
		void set_transparency(float transparency);

		bool is_shadow_casting() const { return !material->get_flag(MaterialFlag::DisableShadowCasting); }
		void set_is_shadow_casting(bool casts_shadows) { return material->set_flag(MaterialFlag::DisableShadowCasting, !casts_shadows); }

		static AssetType get_static_type() { return AssetType::Material; }
		virtual AssetType get_asset_type() const override { return get_static_type(); }

		Reference<Material> get_material() const { return material; }
		void set_material(Reference<Material> in_material) { material = in_material; }

		bool is_transparent() const { return transparent; }

	private:
		void set_defaults();

	private:
		Reference<Material> material;
		bool transparent = false;
	};

	class MaterialTable : public ReferenceCounted {
	public:
		MaterialTable(uint32_t in_material_count = 1);
		MaterialTable(Reference<MaterialTable> other);
		~MaterialTable() = default;

		bool has_material(uint32_t materialIndex) const { return materials.find(materialIndex) != materials.end(); }
		void set_material(uint32_t index, Reference<MaterialAsset> material);
		void clear_material(uint32_t index);

		Reference<MaterialAsset> get_material(uint32_t materialIndex) const
		{
			CORE_ASSERT(has_material(materialIndex), "");
			return materials.at(materialIndex);
		}
		std::unordered_map<uint32_t, Reference<MaterialAsset>>& get_materials() { return materials; }
		const std::unordered_map<uint32_t, Reference<MaterialAsset>>& get_materials() const { return materials; }

		uint32_t get_material_count() const { return material_count; }
		void set_material_count(uint32_t in_material_count) { material_count = in_material_count; }

		void clear();

	private:
		std::unordered_map<uint32_t, Reference<MaterialAsset>> materials;
		uint32_t material_count;
	};

} // namespace ForgottenEngine
