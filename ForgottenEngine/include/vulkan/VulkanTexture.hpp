#pragma once

#include "render/Texture.hpp"

#include <vulkan/vulkan.h>

#include "vulkan/VulkanImage.hpp"

namespace ForgottenEngine {

class VulkanTexture2D : public Texture2D {
public:
	VulkanTexture2D(const std::string& path, TextureProperties properties);
	VulkanTexture2D(
		ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties);
	~VulkanTexture2D() override;
	void resize(const glm::uvec2& size) override;
	void resize(uint32_t width, uint32_t height) override;

	void lock() override;
	void unlock() override;
	Buffer get_writeable_buffer() override;
	bool is_loaded() const override;
	const std::string& get_path() const override;

	ImageFormat get_format() const override { return m_Format; }
	uint32_t get_width() const override { return m_Width; }
	uint32_t get_height() const override { return m_Height; }
	glm::uvec2 get_size() const override { return { m_Width, m_Height }; }

protected:
	void bind_impl(uint32_t slot = 0) const override;

public:
	Reference<Image2D> get_image() const override { return m_Image; }
	const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const
	{
		return m_Image.as<VulkanImage2D>()->GetDescriptorInfo();
	}

	uint32_t get_mip_level_count() const override;
	std::pair<uint32_t, uint32_t> get_mip_size(uint32_t mip) const override;
	uint64_t get_hash() const override { return (uint64_t)m_Image.as<VulkanImage2D>()->GetImageInfo().ImageView; }

	void generate_mips();

private:
	bool load_image(const std::string& path);
	bool load_image(const void* data, uint32_t size);

private:
	std::string m_Path;
	uint32_t m_Width;
	uint32_t m_Height;
	TextureProperties m_Properties;

	Buffer m_ImageData;

	Reference<Image2D> m_Image;

	ImageFormat m_Format = ImageFormat::None;
};

class VulkanTextureCube : public TextureCube {
public:
	VulkanTextureCube(
		ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties);
	VulkanTextureCube(const std::string& path, TextureProperties properties);
	void release();
	~VulkanTextureCube() override;

protected:
	void bind_impl(uint32_t slot) const override;

public:
	ImageFormat get_format() const override { return m_Format; }

	uint32_t get_width() const override { return m_Width; }
	uint32_t get_height() const override { return m_Height; }
	glm::uvec2 get_size() const override { return { m_Width, m_Height }; }

	std::pair<uint32_t, uint32_t> get_mip_size(uint32_t mip) const override;
	uint64_t get_hash() const override;
	uint32_t get_mip_level_count() const override { return 0; }

	const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorImageInfo; }
	VkImageView create_image_view_single_mip(uint32_t mip);

	void generate_mips(bool readonly = false);

private:
	void invalidate();

private:
	ImageFormat m_Format = ImageFormat::None;
	uint32_t m_Width = 0, m_Height = 0;
	TextureProperties m_Properties;

	bool m_MipsGenerated = false;

	Buffer m_LocalStorage;
	VmaAllocation m_MemoryAlloc;
	VkImage m_Image{ nullptr };
	VkDescriptorImageInfo m_DescriptorImageInfo = {};
};

}
