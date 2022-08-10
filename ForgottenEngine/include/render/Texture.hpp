#pragma once

#include "Asset.hpp"
#include "Buffer.hpp"
#include "Common.hpp"
#include "render/Image.hpp"

namespace ForgottenEngine {

	class Texture : public Asset {
	public:
		~Texture() override = default;

		virtual void bind(uint32_t slot = 0) const = 0;

		virtual ImageFormat get_format() const = 0;
		virtual uint32_t get_width() const = 0;
		virtual uint32_t get_height() const = 0;
		virtual glm::uvec2 get_size() const = 0;

		virtual uint32_t get_mip_level_count() const = 0;
		virtual std::pair<uint32_t, uint32_t> get_mip_size(uint32_t mip) const = 0;

		virtual uint64_t get_hash() const = 0;

		virtual TextureType get_type() const = 0;
	};

	class Texture2D : public Texture {
	public:
		virtual void resize(const glm::uvec2& size) = 0;
		virtual void resize(uint32_t width, uint32_t height) = 0;

		virtual Reference<Image2D> get_image() const = 0;

		virtual void lock() = 0;
		virtual void unlock() = 0;

		virtual Buffer get_writeable_buffer() = 0;

		virtual bool is_loaded() const = 0;

		virtual const std::string& get_path() const = 0;

		TextureType get_type() const override { return TextureType::Texture2D; }

		static AssetType get_static_type() { return AssetType::Texture; }
		AssetType get_asset_type() const override { return get_static_type(); }

		static Reference<Texture2D> create(ImageFormat format, uint32_t width, uint32_t height,
			const void* data = nullptr, TextureProperties properties = TextureProperties());
		static Reference<Texture2D> create(
			const std::string& path, TextureProperties properties = TextureProperties());
	};

	class TextureCube : public Texture {
	public:
		TextureType get_type() const override { return TextureType::TextureCube; }

		static AssetType get_static_type() { return AssetType::EnvMap; }
		AssetType get_asset_type() const override { return get_static_type(); }

		static Reference<TextureCube> create(ImageFormat format, uint32_t width, uint32_t height,
			const void* data = nullptr, TextureProperties properties = TextureProperties());
		static Reference<TextureCube> create(
			const std::string& path, TextureProperties properties = TextureProperties());
	};

} // namespace ForgottenEngine
