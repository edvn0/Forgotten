#pragma once

#include "render/Image.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanDevice.hpp"

#include <unordered_map>

namespace ForgottenEngine {

	struct VulkanImageInfo {
		VkImage image = nullptr;
		VkImageView image_view = nullptr;
		VkSampler sampler = nullptr;
		VmaAllocation memory_alloc = nullptr;
	};

	class VulkanImage2D : public Image2D {
	public:
		VulkanImage2D(const ImageSpecification& specification);
		~VulkanImage2D() override;

		void resize(const glm::uvec2& size) override { resize(size.x, size.y); }
		void resize(const uint32_t width, const uint32_t height) override
		{
			specification.Width = width;
			specification.Height = height;
			invalidate();
		}
		void invalidate() override;
		void release() override;
		uint32_t get_width() const override { return specification.Width; }
		uint32_t get_height() const override { return specification.Height; }
		glm::uvec2 get_size() const override { return { specification.Width, specification.Height }; }

		float get_aspect_ratio() const override { return (float)specification.Width / (float)specification.Height; }

		ImageSpecification& get_specification() override { return specification; }
		const ImageSpecification& get_specification() const override { return specification; }

		void rt_invalidate();

		void create_per_layer_image_views() override;
		void rt_create_per_layer_image_views();
		void rt_create_per_specific_layer_image_views(const std::vector<uint32_t>& layer_indices);

		VkImageView get_layer_image_view(uint32_t layer)
		{
			core_assert(layer < per_layer_image_views.size(), "");
			return per_layer_image_views[layer];
		}

		VkImageView get_mip_image_view(uint32_t mip);
		VkImageView rt_get_mip_image_view(uint32_t mip);

		VulkanImageInfo& get_image_info() { return info; }
		const VulkanImageInfo& get_image_info() const { return info; }

		const VkDescriptorImageInfo& get_descriptor_info() const { return descriptor_image_info; }

		Buffer get_buffer() const override { return image_data; }
		Buffer& get_buffer() override { return image_data; }

		uint64_t get_hash() const override { return (uint64_t)info.image; }

		void update_descriptor();

	private:
		ImageSpecification specification;

		Buffer image_data;

		VulkanImageInfo info;

		std::vector<VkImageView> per_layer_image_views;
		std::unordered_map<uint32_t, VkImageView> per_mip_image_views;
		VkDescriptorImageInfo descriptor_image_info = {};
	};

	namespace Utils {

		inline VkFormat vulkan_image_format(ImageFormat format)
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
				core_assert_bool(false);
			}

			return VK_FORMAT_UNDEFINED;
		}

	} // namespace Utils

} // namespace ForgottenEngine
