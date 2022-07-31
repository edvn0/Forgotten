#pragma once

#include "Asset.hpp"
#include "render/Material.hpp"

#include <map>

namespace ForgottenEngine {

class MaterialAsset : public Asset
{
public:
	MaterialAsset(bool transparent = false);
	MaterialAsset(Reference<Material> material);
	~MaterialAsset();

	glm::vec3& GetAlbedoColor();
	void SetAlbedoColor(const glm::vec3& color);

	float& GetMetalness();
	void SetMetalness(float value);

	float& GetRoughness();
	void SetRoughness(float value);

	float& GetEmission();
	void SetEmission(float value);

	// Textures
	Reference<Texture2D> GetAlbedoMap();
	void SetAlbedoMap(Reference<Texture2D> texture);
	void ClearAlbedoMap();

	Reference<Texture2D> GetNormalMap();
	void SetNormalMap(Reference<Texture2D> texture);
	bool IsUsingNormalMap();
	void SetUseNormalMap(bool value);
	void ClearNormalMap();

	Reference<Texture2D> GetMetalnessMap();
	void SetMetalnessMap(Reference<Texture2D> texture);
	void ClearMetalnessMap();

	Reference<Texture2D> GetRoughnessMap();
	void SetRoughnessMap(Reference<Texture2D> texture);
	void ClearRoughnessMap();

	float& GetTransparency();
	void SetTransparency(float transparency);

	bool IsShadowCasting() const { return !m_Material->get_flag(MaterialFlag::DisableShadowCasting); }
	void SetShadowCasting(bool castsShadows) { return m_Material->set_flag(MaterialFlag::DisableShadowCasting, !castsShadows); }

	static AssetType get_static_type() { return AssetType::Material; }
	virtual AssetType get_asset_type() const override { return get_static_type(); }

	Reference<Material> GetMaterial() const { return m_Material; }
	void SetMaterial(Reference<Material> material) { m_Material = material; }

	bool IsTransparent() const { return m_Transparent; }
private:
	void SetDefaults();
private:
	Reference<Material> m_Material;
	bool m_Transparent = false;

	friend class MaterialEditor;
};

class MaterialTable : public ReferenceCounted
{
public:
	MaterialTable(uint32_t materialCount = 1);
	MaterialTable(Reference<MaterialTable> other);
	~MaterialTable() = default;

	bool HasMaterial(uint32_t materialIndex) const { return m_Materials.find(materialIndex) != m_Materials.end(); }
	void SetMaterial(uint32_t index, Reference<MaterialAsset> material);
	void ClearMaterial(uint32_t index);

	Reference<MaterialAsset> GetMaterial(uint32_t materialIndex) const
	{
		CORE_ASSERT(HasMaterial(materialIndex), "");
		return m_Materials.at(materialIndex);
	}
	std::map<uint32_t, Reference<MaterialAsset>>& GetMaterials() { return m_Materials; }
	const std::map<uint32_t, Reference<MaterialAsset>>& GetMaterials() const { return m_Materials; }

	uint32_t GetMaterialCount() const { return m_MaterialCount; }
	void SetMaterialCount(uint32_t materialCount) { m_MaterialCount = materialCount; }

	void Clear();
private:
	std::map<uint32_t, Reference<MaterialAsset>> m_Materials;
	uint32_t m_MaterialCount;
};

}
