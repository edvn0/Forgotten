#pragma once

#include "render/Image.hpp"
#include "vulkan/VulkanContext.hpp"

#include <map>

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

namespace ForgottenEngine {

struct VulkanImageInfo {
	VkImage Image = nullptr;
	VkImageView ImageView = nullptr;
	VkSampler Sampler = nullptr;
	VmaAllocation MemoryAlloc = nullptr;
};

class VulkanImage2D : public Image2D {
public:
	VulkanImage2D(const ImageSpecification& specification);
	~VulkanImage2D() override;

	void resize(const glm::uvec2& size) override { resize(size.x, size.y); }
	void resize(const uint32_t width, const uint32_t height) override
	{
		m_Specification.Width = width;
		m_Specification.Height = height;
		invalidate();
	}
	void invalidate() override;
	void release() override;
	uint32_t get_width() const override { return m_Specification.Width; }
	uint32_t get_height() const override { return m_Specification.Height; }
	glm::uvec2 get_size() const override { return { m_Specification.Width, m_Specification.Height }; }

	float get_aspect_ratio() const override
	{
		return (float)m_Specification.Width / (float)m_Specification.Height;
	}

	ImageSpecification& get_specification() override { return m_Specification; }
	const ImageSpecification& get_specification() const override { return m_Specification; }

	void rt_invalidate();

	void create_per_layer_image_views() override;
	void rt_create_per_layer_image_views();
	void rt_create_per_specific_layer_image_views(const std::vector<uint32_t>& layer_indices);

	VkImageView get_layer_image_view(uint32_t layer)
	{
		CORE_ASSERT(layer < m_PerLayerImageViews.size(), "");
		return m_PerLayerImageViews[layer];
	}

	VkImageView get_mip_image_view(uint32_t mip);
	VkImageView rt_get_mip_image_view(uint32_t mip);

	VulkanImageInfo& get_image_info() { return m_Info; }
	const VulkanImageInfo& get_image_info() const { return m_Info; }

	const VkDescriptorImageInfo& get_descriptor_info() const { return m_DescriptorImageInfo; }

	Buffer get_buffer() const override { return m_ImageData; }
	Buffer& get_buffer() override { return m_ImageData; }

	uint64_t get_hash() const override { return (uint64_t)m_Info.Image; }

	void update_descriptor();

private:
	ImageSpecification m_Specification;

	Buffer m_ImageData;

	VulkanImageInfo m_Info;

	std::vector<VkImageView> m_PerLayerImageViews;
	std::map<uint32_t, VkImageView> m_PerMipImageViews;
	VkDescriptorImageInfo m_DescriptorImageInfo = {};
};

namespace Utils {

	inline VkFormat VulkanImageFormat(ImageFormat format)
	{
		switch (format) {
		case ImageFormat::RED8UN:
			return VK_FORMAT_R8_UNORM;
		case ImageFormat::RED8UI:
			return VK_FORMAT_R8_UINT;
		case ImageFormat::RED16UI:
			return VK_FORMAT_R16_UINT;
		case ImageFormat::RED32UI:
			return VK_FORMAT_R32_UINT;
		case ImageFormat::RED32F:
			return VK_FORMAT_R32_SFLOAT;
		case ImageFormat::RG8:
			return VK_FORMAT_R8G8_UNORM;
		case ImageFormat::RG16F:
			return VK_FORMAT_R16G16_SFLOAT;
		case ImageFormat::RG32F:
			return VK_FORMAT_R32G32_SFLOAT;
		case ImageFormat::RGBA:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case ImageFormat::RGBA16F:
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		case ImageFormat::RGBA32F:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ImageFormat::B10R11G11UF:
			return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case ImageFormat::DEPTH32FSTENCIL8UINT:
			return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case ImageFormat::DEPTH32F:
			return VK_FORMAT_D32_SFLOAT;
		case ImageFormat::DEPTH24STENCIL8:
			return VulkanContext::get_current_device()->get_physical_device()->get_depth_format();
		default:
			CORE_ASSERT(false, "");
		}

		return VK_FORMAT_UNDEFINED;
	}

}

}
