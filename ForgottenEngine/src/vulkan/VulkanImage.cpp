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

		CORE_VERIFY_BOOL(specification.Width > 0 && specification.Height > 0);
	}

	VulkanImage2D::~VulkanImage2D()
	{
		if (info.image) {
			Renderer::submit_resource_free([info = this->info, layer_views = per_layer_image_views]() {
				const auto vk_device = VulkanContext::get_current_device()->get_vulkan_device();
				vkDestroyImageView(vk_device, info.image_view, nullptr);
				vkDestroySampler(vk_device, info.sampler, nullptr);

				for (auto& view : layer_views) {
					if (view)
						vkDestroyImageView(vk_device, view, nullptr);
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
		if (info.image == nullptr)
			return;

		Renderer::submit_resource_free([info = this->info, mip_views = per_mip_image_views, layer_views = per_layer_image_views]() mutable {
			const auto vk_device = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyImageView(vk_device, info.image_view, nullptr);
			vkDestroySampler(vk_device, info.sampler, nullptr);

			for (auto& view : mip_views) {
				if (view.second)
					vkDestroyImageView(vk_device, view.second, nullptr);
			}
			for (auto& view : layer_views) {
				if (view)
					vkDestroyImageView(vk_device, view, nullptr);
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
		CORE_VERIFY_BOOL(specification.Width > 0 && specification.Height > 0);

		// Try release first if necessary
		release();

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		VulkanAllocator allocator("Image2D");

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: this (probably) shouldn't be implied
		if (specification.Usage == ImageUsage::Attachment) {
			if (Utils::IsDepthFormat(specification.Format))
				usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (specification.Transfer || specification.Usage == ImageUsage::Texture) {
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (specification.Usage == ImageUsage::Storage) {
			usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		VkFormat vulkanFormat = Utils::VulkanImageFormat(specification.Format);

		VmaMemoryUsage memoryUsage = specification.Usage == ImageUsage::HostRead ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_GPU_ONLY;

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = vulkanFormat;
		imageCreateInfo.extent.width = specification.Width;
		imageCreateInfo.extent.height = specification.Height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = specification.Mips;
		imageCreateInfo.arrayLayers = specification.Layers;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = specification.Usage == ImageUsage::HostRead ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = usage;
		info.memory_alloc = allocator.allocate_image(imageCreateInfo, memoryUsage, info.image);

		// Create a default image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = specification.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = vulkanFormat;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = specification.Mips;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = specification.Layers;
		imageViewCreateInfo.image = info.image;
		VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &info.image_view));

		// TODO: Renderer should contain some kind of sampler cache
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		if (Utils::IsIntegerBased(specification.Format)) {
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		} else {
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}

		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 100.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &info.sampler));

		if (specification.Usage == ImageUsage::Storage) {
			// Transition image to GENERAL layout
			VkCommandBuffer commandBuffer = VulkanContext::get_current_device()->get_command_buffer(true);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = specification.Mips;
			subresourceRange.layerCount = specification.Layers;

			Utils::insert_image_memory_barrier(commandBuffer, info.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

			VulkanContext::get_current_device()->flush_command_buffer(commandBuffer);
		} else if (specification.Usage == ImageUsage::HostRead) {
			// Transition image to TRANSFER_DST layout
			VkCommandBuffer commandBuffer = VulkanContext::get_current_device()->get_command_buffer(true);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = specification.Mips;
			subresourceRange.layerCount = specification.Layers;

			Utils::insert_image_memory_barrier(commandBuffer, info.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

			VulkanContext::get_current_device()->flush_command_buffer(commandBuffer);
		}

		update_descriptor();
	}

	void VulkanImage2D::create_per_layer_image_views()
	{
		Renderer::submit([this]() mutable { this->rt_create_per_layer_image_views(); });
	}

	void VulkanImage2D::rt_create_per_layer_image_views()
	{
		CORE_ASSERT(specification.Layers > 1, "");

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		const VkFormat vulkanFormat = Utils::VulkanImageFormat(specification.Format);

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
			VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &per_layer_image_views[layer]));
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

			VkImageAspectFlags aspectMask = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			if (specification.Format == ImageFormat::DEPTH24STENCIL8)
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

			VkFormat vulkanFormat = Utils::VulkanImageFormat(specification.Format);
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

			VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &per_mip_image_views[mip]));
		}
		return per_mip_image_views.at(mip);
	}

	void VulkanImage2D::update_descriptor()
	{
		if (specification.Format == ImageFormat::DEPTH24STENCIL8 || specification.Format == ImageFormat::DEPTH32F
			|| specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else if (specification.Usage == ImageUsage::Storage)
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		else
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (specification.Usage == ImageUsage::Storage)
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		else if (specification.Usage == ImageUsage::HostRead)
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		descriptor_image_info.imageView = info.image_view;
		descriptor_image_info.sampler = info.sampler;
	}

	void VulkanImage2D::rt_create_per_specific_layer_image_views(const std::vector<uint32_t>& layer_indices)
	{
		CORE_ASSERT(specification.Layers > 1, "");

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		const VkFormat vulkanFormat = Utils::VulkanImageFormat(specification.Format);

		// HZ_CORE_ASSERT(per_layer_image_views.size() == specification.Layers);
		if (per_layer_image_views.empty())
			per_layer_image_views.resize(specification.Layers);

		for (uint32_t layer : layer_indices) {
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
			VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &per_layer_image_views[layer]));
		}
	}

} // namespace ForgottenEngine
