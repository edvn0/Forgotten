#include "fg_pch.hpp"

#include "vulkan/VulkanTexture.hpp"

#include "render/Renderer.hpp"
#include "stb_image.h"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace ForgottenEngine {

	namespace Utils {

		static VkSamplerAddressMode vulkan_sampler_wrap(TextureWrap wrap)
		{
			switch (wrap) {
			case TextureWrap::Clamp:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case TextureWrap::Repeat:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			default:
				core_assert_bool(false);
			}
			return (VkSamplerAddressMode)0;
		}

		static VkFilter vulkan_sampler_filter(TextureFilter filter)
		{
			switch (filter) {
			case TextureFilter::Linear:
				return VK_FILTER_LINEAR;
			case TextureFilter::Nearest:
				return VK_FILTER_NEAREST;
			case TextureFilter::Cubic:
				return VK_FILTER_CUBIC_IMG;
			default:
				core_assert_bool(false);
			}
			return (VkFilter)0;
		}

		static size_t get_memory_size(ImageFormat format, uint32_t width, uint32_t height)
		{
			switch (format) {
			case ImageFormat::RED16UI:
				return width * height * sizeof(uint16_t);
			case ImageFormat::RG16F:
				return width * height * 2 * sizeof(uint16_t);
			case ImageFormat::RG32F:
				return width * height * 2 * sizeof(float);
			case ImageFormat::RED32F:
				return width * height * sizeof(float);
			case ImageFormat::RED8UN:
				return width * height;
			case ImageFormat::RED8UI:
				return width * height;
			case ImageFormat::RGBA:
				return width * height * 4;
			case ImageFormat::RGBA32F:
				return width * height * 4 * sizeof(float);
			case ImageFormat::B10R11G11UF:
				return width * height * sizeof(float);
			default:
				core_assert_bool(false);
			}

			return 0;
		}

	} // namespace Utils

	//////////////////////////////////////////////////////////////////////////////////
	// Texture2D
	//////////////////////////////////////////////////////////////////////////////////

	VulkanTexture2D::VulkanTexture2D(const std::string& path, const TextureProperties& properties)
		: path(path)
		, properties(properties)
	{
		bool loaded = load_image(path);
		core_assert_bool(loaded);
		if (!loaded) {
			CORE_ERROR("Could not load Texture.");
		}

		ImageSpecification image_spec;
		image_spec.Format = format;
		image_spec.Width = width;
		image_spec.Height = height;
		image_spec.Mips = properties.GenerateMips ? VulkanTexture2D::get_mip_level_count() : 1;
		image_spec.DebugName = properties.DebugName;
		image = Image2D::create(image_spec);

		core_assert_bool(format != ImageFormat::None);

		Reference<VulkanTexture2D> instance = this;
		Renderer::submit([instance]() mutable { instance->invalidate(); });
	}

	VulkanTexture2D::VulkanTexture2D(ImageFormat format, uint32_t width, uint32_t height, const void* data, const TextureProperties& properties)
		: width(width)
		, height(height)
		, properties(properties)
		, format(format)
	{
		auto size = (uint32_t)Utils::get_memory_size(format, width, height);

		if (data)
			image_data = Buffer::copy(data, size);

		ImageSpecification image_spec;
		image_spec.Format = format;
		image_spec.Width = width;
		image_spec.Height = height;
		image_spec.Mips = properties.GenerateMips ? VulkanTexture2D::get_mip_level_count() : 1;
		image_spec.DebugName = properties.DebugName;
		if (properties.Storage)
			image_spec.Usage = ImageUsage::Storage;
		image = Image2D::create(image_spec);

		Reference<VulkanTexture2D> instance = this;
		Renderer::submit([instance]() mutable { instance->invalidate(); });
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		if (image)
			image->release();

		image_data.release();
	}

	bool VulkanTexture2D::load_image(const std::string& in_path)
	{
		int stbi_w, stbi_h, stbi_channels;

		if (stbi_is_hdr(in_path.c_str())) {
			image_data.data = (byte*)stbi_loadf(in_path.c_str(), &stbi_w, &stbi_h, &stbi_channels, 4);
			image_data.size = stbi_w * stbi_h * 4 * sizeof(float);
			format = ImageFormat::RGBA32F;
		} else {
			// stbi_set_flip_vertically_on_load(1);
			image_data.data = stbi_load(in_path.c_str(), &stbi_w, &stbi_h, &stbi_channels, 4);
			image_data.size = stbi_w * stbi_h * 4;
			format = ImageFormat::RGBA;
		}

		core_assert(image_data.data, fmt::format("Failed to load image from in_path: {}.", in_path));

		this->width = static_cast<uint32_t>(stbi_w);
		this->height = static_cast<uint32_t>(stbi_h);
		this->channels = static_cast<uint32_t>(stbi_channels);
		return true;
	}

	void VulkanTexture2D::resize(const glm::uvec2& size) { resize(size.x, size.y); }

	void VulkanTexture2D::resize(const uint32_t w, const uint32_t h)
	{
		this->width = w;
		this->height = h;

		Reference<VulkanTexture2D> instance = this;
		Renderer::submit([instance]() mutable { instance->invalidate(); });
	}

	void VulkanTexture2D::invalidate()
	{
		auto device = VulkanContext::get_current_device();
		auto vulkan_device = device->get_vulkan_device();

		image->release();
		uint32_t mip_count = properties.GenerateMips ? get_mip_level_count() : 1;

		auto& image_spec = image->get_specification();
		image_spec.Format = format;
		image_spec.Width = width;
		image_spec.Height = height;
		image_spec.Mips = mip_count;
		if (!image_data)
			image_spec.Usage = ImageUsage::Storage;

		Reference<VulkanImage2D> reference = this->image.as<VulkanImage2D>();
		reference->rt_invalidate();

		auto& info = reference->get_image_info();

		if (image_data) {
			VkDeviceSize size = image_data.size;

			VkMemoryAllocateInfo mem_alloc_info {};
			mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

			VulkanAllocator allocator("Texture2D");

			// Create staging buffer
			VkBufferCreateInfo buffer_create_info {};
			buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_create_info.size = size;
			buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer staging_buffer;
			VmaAllocation staging_buffer_allocation = allocator.allocate_buffer(buffer_create_info, VMA_MEMORY_USAGE_CPU_TO_GPU, staging_buffer);

			// Copy data to staging buffer
			auto* dest_data = allocator.map_memory<uint8_t>(staging_buffer_allocation);
			core_assert_bool(image_data.data);
			memcpy(dest_data, image_data.data, size);
			allocator.unmap_memory(staging_buffer_allocation);

			VkCommandBuffer copy_cmd = device->get_command_buffer(true);

			// Image memory barriers for the texture reference

			// The sub resource range describes the regions of the reference that will be transitioned using the memory
			// barriers below
			VkImageSubresourceRange subresource_range = {};
			// Image only contains color data
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// Start at first mip level
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = 1;
			subresource_range.layerCount = 1;

			// Transition the texture reference layout to transfer target, so we can safely copy our buffer data to it.
			VkImageMemoryBarrier image_memory_barrier {};
			image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.image = info.image;
			image_memory_barrier.subresourceRange = subresource_range;
			image_memory_barrier.srcAccessMask = 0;
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			vkCmdPipelineBarrier(
				copy_cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

			VkBufferImageCopy buffer_copy_region = {};
			buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			buffer_copy_region.imageSubresource.mipLevel = 0;
			buffer_copy_region.imageSubresource.baseArrayLayer = 0;
			buffer_copy_region.imageSubresource.layerCount = 1;
			buffer_copy_region.imageExtent.width = width;
			buffer_copy_region.imageExtent.height = height;
			buffer_copy_region.imageExtent.depth = 1;
			buffer_copy_region.bufferOffset = 0;

			vkCmdCopyBufferToImage(copy_cmd, staging_buffer, info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_copy_region);

			if (mip_count > 1) // Mips to generate
			{
				Utils::insert_image_memory_barrier(copy_cmd, info.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, subresource_range);
			} else {
				Utils::insert_image_memory_barrier(copy_cmd, info.image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reference->get_descriptor_info().imageLayout, VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, subresource_range);
			}

			device->flush_command_buffer(copy_cmd);

			// Clean up staging resources
			allocator.destroy_buffer(staging_buffer, staging_buffer_allocation);
		} else {
			VkCommandBuffer transition_command_buffer = device->get_command_buffer(true);
			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresource_range.layerCount = 1;
			subresource_range.levelCount = get_mip_level_count();
			Utils::set_image_layout(
				transition_command_buffer, info.image, VK_IMAGE_LAYOUT_UNDEFINED, reference->get_descriptor_info().imageLayout, subresource_range);
			device->flush_command_buffer(transition_command_buffer);
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// CREATE TEXTURE SAMPLER
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Create a texture sampler
		VkSamplerCreateInfo sampler {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = Utils::vulkan_sampler_filter(properties.SamplerFilter);
		sampler.minFilter = Utils::vulkan_sampler_filter(properties.SamplerFilter);
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.addressModeV = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.addressModeW = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = (float)mip_count;
		auto& props = VulkanContext::get_current_device()->get_physical_device()->get_properties();
		if (props.limits.maxSamplerAnisotropy) {
			sampler.maxAnisotropy = 1.0f; // vulkan_device->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		} else {
			sampler.maxAnisotropy = 1.0;
			sampler.anisotropyEnable = VK_FALSE;
		}
		sampler.maxAnisotropy = 1.0;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		vk_check(vkCreateSampler(vulkan_device, &sampler, nullptr, &info.sampler));

		if (!properties.Storage) {
			VkImageViewCreateInfo view {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.format = Utils::vulkan_image_format(format);
			view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			// The subresource range describes the set of mip levels (and array layers) that can be accessed through
			// this reference view It's possible to create multiple reference views for a single reference referring to
			// different (and/or overlapping) ranges of the reference
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = mip_count;
			view.image = info.image;
			vk_check(vkCreateImageView(vulkan_device, &view, nullptr, &info.image_view));

			reference->update_descriptor();
		}

		if (image_data && properties.GenerateMips && mip_count > 1)
			generate_mips();

		stbi_image_free(image_data.data);
		image_data = Buffer();
	}

	void VulkanTexture2D::lock() { }

	void VulkanTexture2D::unlock() { }

	Buffer VulkanTexture2D::get_writeable_buffer() { return image_data; }

	const std::string& VulkanTexture2D::get_path() const { return path; }

	uint32_t VulkanTexture2D::get_mip_level_count() const { return Utils::calculate_mip_count(width, height); }

	std::pair<uint32_t, uint32_t> VulkanTexture2D::get_mip_size(uint32_t mip) const
	{
		uint32_t w = this->width;
		uint32_t h = this->height;
		while (mip != 0) {
			w /= 2;
			h /= 2;
			mip--;
		}

		return { width, height };
	}

	void VulkanTexture2D::generate_mips()
	{
		auto device = VulkanContext::get_current_device();

		auto img = this->image.as<VulkanImage2D>();
		const auto& info = img->get_image_info();

		const VkCommandBuffer blit_cmd = VulkanContext::get_current_device()->get_command_buffer(true);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = info.image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		const auto mip_levels = get_mip_level_count();
		for (uint32_t i = 1; i < mip_levels; i++) {
			VkImageBlit image_blit {};

			// Source
			image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_blit.srcSubresource.layerCount = 1;
			image_blit.srcSubresource.mipLevel = i - 1;
			image_blit.srcOffsets[1].x = int32_t(width >> (i - 1));
			image_blit.srcOffsets[1].y = int32_t(height >> (i - 1));
			image_blit.srcOffsets[1].z = 1;

			// Destination
			image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_blit.dstSubresource.layerCount = 1;
			image_blit.dstSubresource.mipLevel = i;
			image_blit.dstOffsets[1].x = int32_t(width >> i);
			image_blit.dstOffsets[1].y = int32_t(height >> i);
			image_blit.dstOffsets[1].z = 1;

			VkImageSubresourceRange mip_sub_range = {};
			mip_sub_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mip_sub_range.baseMipLevel = i;
			mip_sub_range.levelCount = 1;
			mip_sub_range.layerCount = 1;

			// Prepare current mip level as image blit destination
			Utils::insert_image_memory_barrier(blit_cmd, info.image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mip_sub_range);

			// Blit from previous level
			vkCmdBlitImage(blit_cmd, info.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
				&image_blit, Utils::vulkan_sampler_filter(properties.SamplerFilter));

			// Prepare current mip level as image blit source for next level
			Utils::insert_image_memory_barrier(blit_cmd, info.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, mip_sub_range);
		}

		// After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.layerCount = 1;
		subresource_range.levelCount = mip_levels;

		Utils::insert_image_memory_barrier(blit_cmd, info.image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, subresource_range);

		device->flush_command_buffer(blit_cmd);
	}

	//////////////////////////////////////////////////////////////////////////////////
	// TextureCube
	//////////////////////////////////////////////////////////////////////////////////

	VulkanTextureCube::VulkanTextureCube(ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties)
		: format(format)
		, width(width)
		, height(height)
		, properties(properties)
	{
		if (data) {
			uint32_t size = width * height * 4 * 6; // six layers
			local_storage = Buffer::copy(data, size);
		}

		Reference<VulkanTextureCube> instance = this;
		Renderer::submit([instance]() mutable { instance->invalidate(); });
	}

	VulkanTextureCube::VulkanTextureCube(const std::string& path, TextureProperties properties)
		: properties(properties)
	{
		core_assert(false, "Not implemented");
	}

	void VulkanTextureCube::release()
	{
		if (image == nullptr)
			return;

		Renderer::submit_resource_free([image = image, allocation = memory_alloc, texInfo = descriptor_image_info]() {
			CORE_TRACE("Destroying VulkanTextureCube");
			auto vulkan_device = VulkanContext::get_current_device()->get_vulkan_device();
			vkDestroyImageView(vulkan_device, texInfo.imageView, nullptr);
			vkDestroySampler(vulkan_device, texInfo.sampler, nullptr);

			VulkanAllocator allocator("TextureCube");
			allocator.destroy_image(image, allocation);
		});
		image = nullptr;
		memory_alloc = nullptr;
		descriptor_image_info.imageView = nullptr;
		descriptor_image_info.sampler = nullptr;
	}

	VulkanTextureCube::~VulkanTextureCube() { release(); }

	void VulkanTextureCube::invalidate()
	{
		auto device = VulkanContext::get_current_device();
		auto vulkan_device = device->get_vulkan_device();

		release();

		VkFormat fmt = Utils::vulkan_image_format(this->format);
		uint32_t mip_count = get_mip_level_count();

		VkMemoryAllocateInfo mem_alloc_info {};
		mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		VulkanAllocator allocator("TextureCube");

		// Create optimal tiled target image on the device
		VkImageCreateInfo image_create_info {};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = fmt;
		image_create_info.mipLevels = mip_count;
		image_create_info.arrayLayers = 6;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.extent = { width, height, 1 };
		image_create_info.usage
			= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		memory_alloc = allocator.allocate_image(image_create_info, VMA_MEMORY_USAGE_GPU_ONLY, image);

		descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		// Copy data if present
		if (local_storage) {
			// Create staging buffer
			VkBufferCreateInfo buffer_create_info {};
			buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_create_info.size = local_storage.size;
			buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer staging_buffer;
			VmaAllocation staging_buffer_allocation = allocator.allocate_buffer(buffer_create_info, VMA_MEMORY_USAGE_CPU_TO_GPU, staging_buffer);

			// Copy data to staging buffer
			auto* dest_data = allocator.map_memory<uint8_t>(staging_buffer_allocation);
			memcpy(dest_data, local_storage.data, local_storage.size);
			allocator.unmap_memory(staging_buffer_allocation);

			VkCommandBuffer copy_cmd = device->get_command_buffer(true);

			// Image memory barriers for the texture image

			// The sub resource range describes the regions of the image that will be transitioned using the memory
			// barriers below
			VkImageSubresourceRange subresource_range = {};
			// Image only contains color data
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// Start at first mip level
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = 1;
			subresource_range.layerCount = 6;

			// Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
			VkImageMemoryBarrier image_memory_barrier {};
			image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.image = image;
			image_memory_barrier.subresourceRange = subresource_range;
			image_memory_barrier.srcAccessMask = 0;
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
			// Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
			// Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
			vkCmdPipelineBarrier(
				copy_cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

			VkBufferImageCopy buffer_copy_region = {};
			buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			buffer_copy_region.imageSubresource.mipLevel = 0;
			buffer_copy_region.imageSubresource.baseArrayLayer = 0;
			buffer_copy_region.imageSubresource.layerCount = 6;
			buffer_copy_region.imageExtent.width = width;
			buffer_copy_region.imageExtent.height = height;
			buffer_copy_region.imageExtent.depth = 1;
			buffer_copy_region.bufferOffset = 0;

			// Copy mip levels from staging buffer
			vkCmdCopyBufferToImage(copy_cmd, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_copy_region);

			Utils::insert_image_memory_barrier(copy_cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, subresource_range);

			device->flush_command_buffer(copy_cmd);

			allocator.destroy_buffer(staging_buffer, staging_buffer_allocation);
		}

		VkCommandBuffer layout_cmd = device->get_command_buffer(true);

		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = mip_count;
		subresource_range.layerCount = 6;

		Utils::set_image_layout(layout_cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, descriptor_image_info.imageLayout, subresource_range);

		device->flush_command_buffer(layout_cmd);

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// CREATE TEXTURE SAMPLER
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Create a texture sampler
		VkSamplerCreateInfo sampler {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = Utils::vulkan_sampler_filter(properties.SamplerFilter);
		sampler.minFilter = Utils::vulkan_sampler_filter(properties.SamplerFilter);
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.addressModeV = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.addressModeW = Utils::vulkan_sampler_wrap(properties.SamplerWrap);
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		// Set max level-of-detail to mip level count of the texture
		sampler.maxLod = (float)mip_count;
		// Enable anisotropic filtering
		// This feature is optional, so we must check if it's supported on the device

		sampler.maxAnisotropy = 1.0;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vk_check(vkCreateSampler(vulkan_device, &sampler, nullptr, &descriptor_image_info.sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo view {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = fmt;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		// The subresource range describes the set of mip levels (and array layers) that can be accessed through this
		// image view It's possible to create multiple image views for a single image referring to different (and/or
		// overlapping) ranges of the image
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = mip_count;
		view.image = image;
		vk_check(vkCreateImageView(vulkan_device, &view, nullptr, &descriptor_image_info.imageView));
	}

	uint32_t VulkanTextureCube::get_mip_level_count() const { return Utils::calculate_mip_count(width, height); }

	std::pair<uint32_t, uint32_t> VulkanTextureCube::get_mip_size(uint32_t mip) const
	{
		uint32_t w = this->width;
		uint32_t h = this->height;
		while (mip != 0) {
			w /= 2;
			h /= 2;
			mip--;
		}

		return { width, height };
	}

	VkImageView VulkanTextureCube::create_image_view_single_mip(uint32_t mip)
	{

		auto device = VulkanContext::get_current_device();
		auto vulkan_device = device->get_vulkan_device();

		VkFormat fmt = Utils::vulkan_image_format(this->format);

		VkImageViewCreateInfo view {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = fmt;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = mip;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = 1;
		view.image = image;

		VkImageView result;
		vk_check(vkCreateImageView(vulkan_device, &view, nullptr, &result));

		return result;
	}

	void VulkanTextureCube::generate_mips(bool readonly)
	{
		auto device = VulkanContext::get_current_device();

		VkCommandBuffer blit_cmd = device->get_command_buffer(true);

		uint32_t mip_levels = get_mip_level_count();
		for (uint32_t face = 0; face < 6; face++) {
			VkImageSubresourceRange mip_sub_range = {};
			mip_sub_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mip_sub_range.baseMipLevel = 0;
			mip_sub_range.baseArrayLayer = face;
			mip_sub_range.levelCount = 1;
			mip_sub_range.layerCount = 1;

			// Prepare current mip level as image blit destination
			Utils::insert_image_memory_barrier(blit_cmd, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mip_sub_range);
		}

		for (auto i = 1; i < mip_levels; i++) {
			for (auto face = 0; face < 6; face++) {
				VkImageBlit image_blit {};

				// Source
				image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				image_blit.srcSubresource.layerCount = 1;
				image_blit.srcSubresource.mipLevel = i - 1;
				image_blit.srcSubresource.baseArrayLayer = face;
				image_blit.srcOffsets[1].x = int32_t(width >> (i - 1));
				image_blit.srcOffsets[1].y = int32_t(height >> (i - 1));
				image_blit.srcOffsets[1].z = 1;

				// Destination
				image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				image_blit.dstSubresource.layerCount = 1;
				image_blit.dstSubresource.mipLevel = i;
				image_blit.dstSubresource.baseArrayLayer = face;
				image_blit.dstOffsets[1].x = int32_t(width >> i);
				image_blit.dstOffsets[1].y = int32_t(height >> i);
				image_blit.dstOffsets[1].z = 1;

				VkImageSubresourceRange mip_sub_range = {};
				mip_sub_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				mip_sub_range.baseMipLevel = i;
				mip_sub_range.baseArrayLayer = face;
				mip_sub_range.levelCount = 1;
				mip_sub_range.layerCount = 1;

				// Prepare current mip level as image blit destination
				Utils::insert_image_memory_barrier(blit_cmd, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mip_sub_range);

				// Blit from previous level
				vkCmdBlitImage(blit_cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit,
					VK_FILTER_LINEAR);

				// Prepare current mip level as image blit source for next level
				Utils::insert_image_memory_barrier(blit_cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, mip_sub_range);
			}
		}

		// After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.layerCount = 6;
		subresource_range.levelCount = mip_levels;

		Utils::insert_image_memory_barrier(blit_cmd, image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readonly ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, subresource_range);

		device->flush_command_buffer(blit_cmd);

		mips_generated = true;

		descriptor_image_info.imageLayout = readonly ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	}

} // namespace ForgottenEngine
