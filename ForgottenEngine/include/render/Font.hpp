#pragma once

#include "render/Texture.hpp"

#include <filesystem>

namespace ForgottenEngine {

	struct MSDFData;

	class Font : public Asset {
	public:
		Font(const std::filesystem::path& filepath);
		virtual ~Font();

		Reference<Texture2D> get_font_atlas() const { return m_TextureAtlas; }
		const MSDFData* get_msdf_data() const { return m_MSDFData; }

		static void init();
		static void shutdown();
		static Reference<Font> get_default_font();

		static AssetType get_static_type() { return AssetType::Font; }
		AssetType get_asset_type() const override { return get_static_type(); }

	private:
		std::filesystem::path m_FilePath;
		Reference<Texture2D> m_TextureAtlas;
		MSDFData* m_MSDFData = nullptr;

	private:
		static Reference<Font> s_DefaultFont;
	};

} // namespace ForgottenEngine
