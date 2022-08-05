#include "fg_pch.hpp"

#include "vulkan/VulkanImage.hpp"

#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace ForgottenEngine {

VulkanImage2D::VulkanImage2D(const ImageSpecification& specification)
	: m_Specification(specification)
{
	CORE_VERIFY(m_Specification.Width > 0 && m_Specification.Height > 0, "");
}

VulkanImage2D::~VulkanImage2D()
{
	if (m_Info.Image) {
		const VulkanImageInfo& info = m_Info;
		Renderer::submit_resource_free([info, layerViews = m_PerLayerImageViews]() {
			const auto vulkanDevice = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyImageView(vulkanDevice, info.ImageView, nullptr);
			vkDestroySampler(vulkanDevice, info.Sampler, nullptr);

			for (auto& view : layerViews) {
				if (view)
					vkDestroyImageView(vulkanDevice, view, nullptr);
			}

			VulkanAllocator allocator("VulkanImage2D");
			allocator.destroy_image(info.Image, info.MemoryAlloc);

			CORE_WARN("Renderer: VulkanImage2D::Release ImageView = {0}", (const void*)info.ImageView);
		});
		m_PerLayerImageViews.clear();
	}
}

void VulkanImage2D::invalidate()
{
	Renderer::submit([this]() mutable { this->rt_invalidate(); });
}

void VulkanImage2D::release()
{
	if (m_Info.Image == nullptr)
		return;

	const VulkanImageInfo& info = m_Info;
	Renderer::submit_resource_free(
		[info, mipViews = m_PerMipImageViews, layerViews = m_PerLayerImageViews]() mutable {
			const auto vulkanDevice = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyImageView(vulkanDevice, info.ImageView, nullptr);
			vkDestroySampler(vulkanDevice, info.Sampler, nullptr);

			for (auto& view : mipViews) {
				if (view.second)
					vkDestroyImageView(vulkanDevice, view.second, nullptr);
			}
			for (auto& view : layerViews) {
				if (view)
					vkDestroyImageView(vulkanDevice, view, nullptr);
			}
			VulkanAllocator allocator("VulkanImage2D");
			allocator.destroy_image(info.Image, info.MemoryAlloc);
		});
	m_Info.Image = nullptr;
	m_Info.ImageView = nullptr;
	m_Info.Sampler = nullptr;
	m_PerLayerImageViews.clear();
	m_PerMipImageViews.clear();
}

void VulkanImage2D::rt_invalidate()
{
	CORE_VERIFY(m_Specification.Width > 0 && m_Specification.Height > 0, "");

	// Try release first if necessary
	release();

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
	VulkanAllocator allocator("Image2D");

	VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: this (probably) shouldn't be implied
	if (m_Specification.Usage == ImageUsage::Attachment) {
		if (Utils::IsDepthFormat(m_Specification.Format))
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (m_Specification.Transfer || m_Specification.Usage == ImageUsage::Texture) {
		usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (m_Specification.Usage == ImageUsage::Storage) {
		usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VkImageAspectFlags aspectMask
		= Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

	VmaMemoryUsage memoryUsage
		= m_Specification.Usage == ImageUsage::HostRead ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_GPU_ONLY;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = vulkanFormat;
	imageCreateInfo.extent.width = m_Specification.Width;
	imageCreateInfo.extent.height = m_Specification.Height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = m_Specification.Mips;
	imageCreateInfo.arrayLayers = m_Specification.Layers;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling
		= m_Specification.Usage == ImageUsage::HostRead ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usage;
	m_Info.MemoryAlloc = allocator.allocate_image(imageCreateInfo, memoryUsage, m_Info.Image);

	// Create a default image view
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.viewType
		= m_Specification.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = vulkanFormat;
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.subresourceRange = {};
	imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = m_Specification.Layers;
	imageViewCreateInfo.image = m_Info.Image;
	VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_Info.ImageView));

	// TODO: Renderer should contain some kind of sampler cache
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	if (Utils::IsIntegerBased(m_Specification.Format)) {
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
	VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_Info.Sampler));

	if (m_Specification.Usage == ImageUsage::Storage) {
		// Transition image to GENERAL layout
		VkCommandBuffer commandBuffer = VulkanContext::get_current_device()->get_command_buffer(true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = m_Specification.Mips;
		subresourceRange.layerCount = m_Specification.Layers;

		Utils::insert_image_memory_barrier(commandBuffer, m_Info.Image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			subresourceRange);

		VulkanContext::get_current_device()->flush_command_buffer(commandBuffer);
	} else if (m_Specification.Usage == ImageUsage::HostRead) {
		// Transition image to TRANSFER_DST layout
		VkCommandBuffer commandBuffer = VulkanContext::get_current_device()->get_command_buffer(true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = m_Specification.Mips;
		subresourceRange.layerCount = m_Specification.Layers;

		Utils::insert_image_memory_barrier(commandBuffer, m_Info.Image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

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
	CORE_ASSERT(m_Specification.Layers > 1, "");

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

	VkImageAspectFlags aspectMask
		= Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	const VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

	m_PerLayerImageViews.resize(m_Specification.Layers);
	for (uint32_t layer = 0; layer < m_Specification.Layers; layer++) {
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = vulkanFormat;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_Info.Image;
		VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerLayerImageViews[layer]));
	}
}

VkImageView VulkanImage2D::get_mip_image_view(uint32_t mip)
{
	if (m_PerMipImageViews.find(mip) == m_PerMipImageViews.end()) {
		Renderer::submit([this, mip]() mutable { this->rt_get_mip_image_view(mip); });
		return nullptr;
	}

	return m_PerMipImageViews.at(mip);
}

VkImageView VulkanImage2D::rt_get_mip_image_view(const uint32_t mip)
{
	if (m_PerMipImageViews.find(mip) == m_PerMipImageViews.end()) {
		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkImageAspectFlags aspectMask
			= Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);
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
		imageViewCreateInfo.image = m_Info.Image;

		VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerMipImageViews[mip]));
	}
	return m_PerMipImageViews.at(mip);
}

void VulkanImage2D::update_descriptor()
{
	if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8 || m_Specification.Format == ImageFormat::DEPTH32F
		|| m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	else if (m_Specification.Usage == ImageUsage::Storage)
		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	else
		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (m_Specification.Usage == ImageUsage::Storage)
		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	else if (m_Specification.Usage == ImageUsage::HostRead)
		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	m_DescriptorImageInfo.imageView = m_Info.ImageView;
	m_DescriptorImageInfo.sampler = m_Info.Sampler;
}

void VulkanImage2D::rt_create_per_specific_layer_image_views(const std::vector<uint32_t>& layer_indices)
{
	CORE_ASSERT(m_Specification.Layers > 1, "");

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

	VkImageAspectFlags aspectMask
		= Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	const VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

	// HZ_CORE_ASSERT(m_PerLayerImageViews.size() == m_Specification.Layers);
	if (m_PerLayerImageViews.empty())
		m_PerLayerImageViews.resize(m_Specification.Layers);

	for (uint32_t layer : layer_indices) {
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = vulkanFormat;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_Info.Image;
		VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerLayerImageViews[layer]));
	}
}

}
