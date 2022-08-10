#pragma once

#include "Assets.hpp"
#include "Common.hpp"
#include "vendor/spirv-cross/spirv_common.hpp"

namespace ForgottenEngine {

	enum class AssetFlag : uint16_t { None = 0,
		Missing = BIT(0),
		Invalid = BIT(1) };

	enum class AssetType : uint16_t {
		None = 0,
		Scene = 1,
		Prefab = 2,
		Mesh = 3,
		StaticMesh = 4,
		MeshSource = 5,
		Material = 6,
		Texture = 7,
		EnvMap = 8,
		Audio = 9,
		PhysicsMat = 10,
		SoundConfig = 11,
		SpatializationConfig = 12,
		Font = 13,
		Script = 14,
		MeshCollider = 15,
		SoundGraphSound = 16,
	};

	namespace Utils {

		inline AssetType asset_type_from_string(const std::string& asset_type)
		{
			if (asset_type == "None")
				return AssetType::None;
			if (asset_type == "Scene")
				return AssetType::Scene;
			if (asset_type == "Prefab")
				return AssetType::Prefab;
			if (asset_type == "Mesh")
				return AssetType::Mesh;
			if (asset_type == "StaticMesh")
				return AssetType::StaticMesh;
			if (asset_type == "MeshAsset")
				return AssetType::MeshSource; // DEPRECATED
			if (asset_type == "MeshSource")
				return AssetType::MeshSource;
			if (asset_type == "Material")
				return AssetType::Material;
			if (asset_type == "Texture")
				return AssetType::Texture;
			if (asset_type == "EnvMap")
				return AssetType::EnvMap;
			if (asset_type == "Audio")
				return AssetType::Audio;
			if (asset_type == "PhysicsMat")
				return AssetType::PhysicsMat;
			if (asset_type == "SoundConfig")
				return AssetType::SoundConfig;
			if (asset_type == "Font")
				return AssetType::Font;
			if (asset_type == "Script")
				return AssetType::Script;
			if (asset_type == "MeshCollider")
				return AssetType::MeshCollider;
			if (asset_type == "SoundGraphSound")
				return AssetType::SoundGraphSound;

			CORE_ASSERT(false, "Unknown Asset Type");
			return AssetType::None;
		}

		inline const char* asset_type_to_string(AssetType asset_type)
		{
			switch (asset_type) {
			case AssetType::None:
				return "None";
			case AssetType::Scene:
				return "Scene";
			case AssetType::Prefab:
				return "Prefab";
			case AssetType::Mesh:
				return "Mesh";
			case AssetType::StaticMesh:
				return "StaticMesh";
			case AssetType::MeshSource:
				return "MeshSource";
			case AssetType::Material:
				return "Material";
			case AssetType::Texture:
				return "Texture";
			case AssetType::EnvMap:
				return "EnvMap";
			case AssetType::Audio:
				return "Audio";
			case AssetType::PhysicsMat:
				return "PhysicsMat";
			case AssetType::SoundConfig:
				return "SoundConfig";
			case AssetType::Font:
				return "Font";
			case AssetType::Script:
				return "Script";
			case AssetType::MeshCollider:
				return "MeshCollider";
			case AssetType::SoundGraphSound:
				return "SoundGraphSound";
			default:
				CORE_ASSERT(false, "Unknown Asset Type");
			}
		}
	} // namespace Utils
} // namespace ForgottenEngine
