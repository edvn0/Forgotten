#pragma once

#include "render/Texture.hpp"

#include <filesystem>

namespace ForgottenEngine {

	struct MSDFData;

	class Font : public Asset {
	public:
		Font(const std::filesystem::path& filepath);
		virtual ~Font();

		Reference<Texture2D> get_font_atlas() const { return texture_atlas; }
		const MSDFData* get_msdf_data() const { return msdf_data; }

		static void init();
		static void shutdown();
		static Reference<Font> get_default_font();

		static AssetType get_static_type() { return AssetType::Font; }
		AssetType get_asset_type() const override { return get_static_type(); }

	private:
		std::filesystem::path file_path;
		Reference<Texture2D> texture_atlas;
		MSDFData* msdf_data = nullptr;

	private:
		static Reference<Font> default_font;
	};

} // namespace ForgottenEngine
