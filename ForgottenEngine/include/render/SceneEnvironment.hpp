#pragma once

#include "render/Texture.hpp"

namespace ForgottenEngine {

	class SceneEnvironment : public Asset {
	public:
		Reference<TextureCube> radiance_map;
		Reference<TextureCube> irradiance_map;

		SceneEnvironment() = default;
		SceneEnvironment(const Reference<TextureCube>& radianceMap, const Reference<TextureCube>& irradianceMap)
			: radiance_map(radianceMap)
			, irradiance_map(irradianceMap)
		{
		}

		static AssetType get_static_type() { return AssetType::EnvMap; }
		virtual AssetType get_asset_type() const override { return get_static_type(); }
	};

} // namespace ForgottenEngine
