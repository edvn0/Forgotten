#pragma once

#include "render/Texture.hpp"
#include "vulkan/VulkanImage.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

	class VulkanTexture2D : public Texture2D {
	public:
		VulkanTexture2D(const std::string& path, TextureProperties properties);
		VulkanTexture2D(ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties);
		~VulkanTexture2D() override;
		void resize(const glm::uvec2& size) override;
		void resize(uint32_t width, uint32_t height) override;

		void invalidate();

		void bind(uint32_t slot) const override {};
		void lock() override;
		void unlock() override;
		Buffer get_writeable_buffer() override;
		bool is_loaded() const override { return image_data; };
		const std::string& get_path() const override;

		const VkDescriptorImageInfo& get_vulkan_descriptor_info() const { return image.as<VulkanImage2D>()->get_descriptor_info(); }

		ImageFormat get_format() const override { return format; }
		uint32_t get_width() const override { return width; }
		uint32_t get_height() const override { return height; }
		glm::uvec2 get_size() const override { return { width, height }; }

	public:
		Reference<Image2D> get_image() const override { return image; }
		uint32_t get_mip_level_count() const override;
		std::pair<uint32_t, uint32_t> get_mip_size(uint32_t mip) const override;
		uint64_t get_hash() const override { return (uint64_t)image.as<VulkanImage2D>()->get_image_info().image_view; }

		void generate_mips();

	private:
		bool load_image(const std::string& path);
		bool load_image(const void* data, uint32_t size);

	private:
		std::string path;
		uint32_t width;
		uint32_t height;
		TextureProperties properties;

		Buffer image_data;

		Reference<Image2D> image;

		ImageFormat format = ImageFormat::None;
	};

	class VulkanTextureCube : public TextureCube {
	public:
		VulkanTextureCube(ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties);
		VulkanTextureCube(const std::string& path, TextureProperties properties);
		void release();
		~VulkanTextureCube() override;

		void bind(uint32_t slot) const override {};

	public:
		ImageFormat get_format() const override { return format; }

		uint32_t get_width() const override { return width; }
		uint32_t get_height() const override { return height; }
		glm::uvec2 get_size() const override { return { width, height }; }

		std::pair<uint32_t, uint32_t> get_mip_size(uint32_t mip) const override;
		uint32_t get_mip_level_count() const override;

		uint64_t get_hash() const override { return (uint64_t)image; }

		const VkDescriptorImageInfo& get_vulkan_descriptor_info() const { return descriptor_image_info; }
		VkImageView create_image_view_single_mip(uint32_t mip);

		void generate_mips(bool readonly = false);

	private:
		void invalidate();

	private:
		ImageFormat format = ImageFormat::None;
		uint32_t width = 0, height = 0;
		TextureProperties properties;

		bool mips_generated = false;

		Buffer local_storage;
		VmaAllocation memory_alloc;
		VkImage image { nullptr };
		VkDescriptorImageInfo descriptor_image_info = {};
	};

} // namespace ForgottenEngine
