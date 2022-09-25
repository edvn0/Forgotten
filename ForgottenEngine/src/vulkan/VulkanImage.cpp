#include "fg_pch.hpp"

#include "vulkan/VulkanImage.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace ForgottenEngine {

	VulkanImage2D::VulkanImage2D(const ImageSpecification& specification)
		: specification(specification)
	{
		CORE_TRACE("{} by {}", specification.Width, specification.Height);

		core_verify_bool(specification.Width > 0 && specification.Height > 0);
	}

	VulkanImage2D::~VulkanImage2D()
	{
		if (info.image) {
			Renderer::submit_resource_free([info = this->info, layer_views = per_layer_image_views]() {
				const auto vk_device = VulkanContext::get_current_device()->get_vulkan_device();
				vkDestroyImageView(vk_device, info.image_view, nullptr);
				vkDestroySampler(vk_device, info.sampler, nullptr);

				for (auto& view : layer_views) {
					if (view) {
						vkDestroyImageView(vk_device, view, nullptr);
					}
				}

				VulkanAllocator allocator("VulkanImage2D");
				allocator.destroy_image(info.image, info.memory_alloc);

				CORE_WARN("Renderer: VulkanImage2D::Release ImageView = {0}", (const void*)info.image_view);
			});
			per_layer_image_views.clear();
		}
	}

	void VulkanImage2D::invalidate()
	{
		Renderer::submit([this]() mutable { this->rt_invalidate(); });
	}

	void VulkanImage2D::release()
	{
		if (info.image == nullptr) {
			return;
		}

		Renderer::submit_resource_free([info = this->info, mip_views = per_mip_image_views, layer_views = per_layer_image_views]() mutable {
			const auto vk_device = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyImageView(vk_device, info.image_view, nullptr);
			vkDestroySampler(vk_device, info.sampler, nullptr);

			for (auto& view : mip_views) {
				if (view.second) {
					vkDestroyImageView(vk_device, view.second, nullptr);
				}
			}
			for (auto& view : layer_views) {
				if (view) {
					vkDestroyImageView(vk_device, view, nullptr);
				}
			}
			VulkanAllocator allocator("VulkanImage2D");
			allocator.destroy_image(info.image, info.memory_alloc);
		});
		info.image = nullptr;
		info.image_view = nullptr;
		info.sampler = nullptr;
		per_layer_image_views.clear();
		per_mip_image_views.clear();
	}

	void VulkanImage2D::rt_invalidate()
	{
		core_verify_bool(specification.Width > 0 && specification.Height > 0);

		// Try release first if necessary
		release();

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		VulkanAllocator allocator("Image2D");

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: this (probably) shouldn't be implied
		if (specification.Usage == ImageUsage::Attachment) {
			if (Utils::is_depth_format(specification.Format)) {
				usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			} else {
				usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
		}
		if (specification.Transfer || specification.Usage == ImageUsage::Texture) {
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (specification.Usage == ImageUsage::Storage) {
			usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VkImageAspectFlags aspect_mask = Utils::is_depth_format(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8) {
			aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		VkFormat vulkan_format = Utils::vulkan_image_format(specification.Format);

		VmaMemoryUsage memory_usage = specification.Usage == ImageUsage::HostRead ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_GPU_ONLY;

		VkImageCreateInfo image_create_info = {};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = vulkan_format;
		image_create_info.extent.width = specification.Width;
		image_create_info.extent.height = specification.Height;
		image_create_info.extent.depth = 1;
		image_create_info.mipLevels = specification.Mips;
		image_create_info.arrayLayers = specification.Layers;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = specification.Usage == ImageUsage::HostRead ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = usage;
		info.memory_alloc = allocator.allocate_image(image_create_info, memory_usage, info.image);

		// Create a default image view
		VkImageViewCreateInfo image_view_create_info = {};
		image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.viewType = specification.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.format = vulkan_format;
		image_view_create_info.flags = 0;
		image_view_create_info.subresourceRange = {};
		image_view_create_info.subresourceRange.aspectMask = aspect_mask;
		image_view_create_info.subresourceRange.baseMipLevel = 0;
		image_view_create_info.subresourceRange.levelCount = specification.Mips;
		image_view_create_info.subresourceRange.baseArrayLayer = 0;
		image_view_create_info.subresourceRange.layerCount = specification.Layers;
		image_view_create_info.image = info.image;
		vk_check(vkCreateImageView(device, &image_view_create_info, nullptr, &info.image_view));

		// TODO: Renderer should contain some kind of sampler cache
		VkSamplerCreateInfo sampler_create_info = {};
		sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_create_info.maxAnisotropy = 1.0f;
		if (Utils::is_integer_based(specification.Format)) {
			sampler_create_info.magFilter = VK_FILTER_NEAREST;
			sampler_create_info.minFilter = VK_FILTER_NEAREST;
			sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		} else {
			sampler_create_info.magFilter = VK_FILTER_LINEAR;
			sampler_create_info.minFilter = VK_FILTER_LINEAR;
			sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}

		sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_create_info.addressModeV = sampler_create_info.addressModeU;
		sampler_create_info.addressModeW = sampler_create_info.addressModeU;
		sampler_create_info.mipLodBias = 0.0f;
		sampler_create_info.maxAnisotropy = 1.0f;
		sampler_create_info.minLod = 0.0f;
		sampler_create_info.maxLod = 100.0f;
		sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vk_check(vkCreateSampler(device, &sampler_create_info, nullptr, &info.sampler));

		if (specification.Usage == ImageUsage::Storage) {
			// Transition image to GENERAL layout
			VkCommandBuffer command_buffer = VulkanContext::get_current_device()->get_command_buffer(true);

			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = specification.Mips;
			subresource_range.layerCount = specification.Layers;

			Utils::insert_image_memory_barrier(command_buffer, info.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresource_range);

			VulkanContext::get_current_device()->flush_command_buffer(command_buffer);
		} else if (specification.Usage == ImageUsage::HostRead) {
			// Transition image to TRANSFER_DST layout
			VkCommandBuffer command_buffer = VulkanContext::get_current_device()->get_command_buffer(true);

			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = specification.Mips;
			subresource_range.layerCount = specification.Layers;

			Utils::insert_image_memory_barrier(command_buffer, info.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresource_range);

			VulkanContext::get_current_device()->flush_command_buffer(command_buffer);
		}

		update_descriptor();
	}

	void VulkanImage2D::create_per_layer_image_views()
	{
		Renderer::submit([this]() mutable { this->rt_create_per_layer_image_views(); });
	}

	void VulkanImage2D::rt_create_per_layer_image_views()
	{
		core_assert(specification.Layers > 1, "");

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkImageAspectFlags aspectMask = Utils::is_depth_format(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8) {
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		const VkFormat vulkanFormat = Utils::vulkan_image_format(specification.Format);

		per_layer_image_views.resize(specification.Layers);
		for (uint32_t layer = 0; layer < specification.Layers; layer++) {
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = vulkanFormat;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.subresourceRange = {};
			imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = specification.Mips;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.image = info.image;
			vk_check(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &per_layer_image_views[layer]));
		}
	}

	VkImageView VulkanImage2D::get_mip_image_view(uint32_t mip)
	{
		if (per_mip_image_views.find(mip) == per_mip_image_views.end()) {
			Renderer::submit([this, mip]() mutable { this->rt_get_mip_image_view(mip); });
			return nullptr;
		}

		return per_mip_image_views.at(mip);
	}

	VkImageView VulkanImage2D::rt_get_mip_image_view(const uint32_t mip)
	{
		if (per_mip_image_views.find(mip) == per_mip_image_views.end()) {
			VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

			VkImageAspectFlags aspectMask = Utils::is_depth_format(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			if (specification.Format == ImageFormat::DEPTH24STENCIL8) {
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			VkFormat vulkanFormat = Utils::vulkan_image_format(specification.Format);
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = vulkanFormat;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.subresourceRange = {};
			imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
			imageViewCreateInfo.subresourceRange.baseMipLevel = mip;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.image = info.image;

			vk_check(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &per_mip_image_views[mip]));
		}
		return per_mip_image_views.at(mip);
	}

	void VulkanImage2D::update_descriptor()
	{
		if (specification.Format == ImageFormat::DEPTH24STENCIL8 || specification.Format == ImageFormat::DEPTH32F
			|| specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT) {
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		} else if (specification.Usage == ImageUsage::Storage) {
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		} else {
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		if (specification.Usage == ImageUsage::Storage) {
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		} else if (specification.Usage == ImageUsage::HostRead) {
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}

		descriptor_image_info.imageView = info.image_view;
		descriptor_image_info.sampler = info.sampler;
	}

	void VulkanImage2D::rt_create_per_specific_layer_image_views(const std::vector<uint32_t>& layer_indices)
	{
		core_assert_bool(specification.Layers > 1);

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkImageAspectFlags aspect_mask = Utils::is_depth_format(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8) {
			aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		const VkFormat vulkan_format = Utils::vulkan_image_format(specification.Format);

		// HZ_core_assert(per_layer_image_views.size() == specification.Layers);
		if (per_layer_image_views.empty()) {
			per_layer_image_views.resize(specification.Layers);
		}

		for (uint32_t layer : layer_indices) {
			VkImageViewCreateInfo image_view_create_info = {};
			image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			image_view_create_info.format = vulkan_format;
			image_view_create_info.flags = 0;
			image_view_create_info.subresourceRange = {};
			image_view_create_info.subresourceRange.aspectMask = aspect_mask;
			image_view_create_info.subresourceRange.baseMipLevel = 0;
			image_view_create_info.subresourceRange.levelCount = specification.Mips;
			image_view_create_info.subresourceRange.baseArrayLayer = layer;
			image_view_create_info.subresourceRange.layerCount = 1;
			image_view_create_info.image = info.image;
			vk_check(vkCreateImageView(device, &image_view_create_info, nullptr, &per_layer_image_views[layer]));
		}
	}

} // namespace ForgottenEngine
