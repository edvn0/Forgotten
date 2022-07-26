#include "fg_pch.hpp"

#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanInitializers.hpp"

#include "vulkan/VulkanContext.hpp"

#include "stb_image.h"

namespace ForgottenEngine {

VulkanImage::VulkanImage(const std::string& path)
{
	auto correct_path = Assets::find_resources_by_path(path);

	CORE_ASSERT(correct_path, "Could not find asset at {}.", path);

	auto path_str = (*correct_path).c_str();
	pixels = stbi_load(path_str, &w, &h, &c, STBI_rgb_alpha);
	;
}

VulkanImage::~VulkanImage() = default;

void VulkanImage::upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context)
{
	void* pixel_ptr = pixels;
	VkDeviceSize image_size = w * h * 4;

	VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

	VulkanBuffer staging{ allocator };
	staging.upload(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data;
	vmaMapMemory(allocator, staging.get_buffer().allocation, &data);
	memcpy(data, pixel_ptr, static_cast<size_t>(image_size));
	vmaUnmapMemory(allocator, staging.get_buffer().allocation);

	stbi_image_free(pixels);

	VkExtent3D extent;
	extent.width = static_cast<uint32_t>(w);
	extent.height = static_cast<uint32_t>(h);
	extent.depth = 1;

	VkImageCreateInfo img_info = VI::Image::image_create_info(
		image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);

	AllocatedImage new_image{ nullptr, nullptr };

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	// allocate and create the image
	vmaCreateImage(allocator, &img_info, &dimg_allocinfo, &new_image.image, &new_image.allocation, nullptr);

	auto& vb = staging.get_buffer();

	upload_context.immediate_submit([&](VkCommandBuffer cmd) {
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier transfer_barrier = {};
		transfer_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		transfer_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		transfer_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		transfer_barrier.image = new_image.image;
		transfer_barrier.subresourceRange = range;

		transfer_barrier.srcAccessMask = 0;
		transfer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// barrier the image into the transfer-receive layout
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
			0, nullptr, 1, &transfer_barrier);

		VkBufferImageCopy copy_region = {};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;

		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = extent;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, staging.get_buffer().buffer, new_image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		auto readable_barrier = transfer_barrier;

		readable_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		readable_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		readable_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		readable_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// barrier the image into the shader readable layout
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
			nullptr, 0, nullptr, 1, &readable_barrier);
	});

	VkImageViewCreateInfo ii
		= VI::Image::image_view_create_info(VK_FORMAT_R8G8B8A8_SRGB, new_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(VulkanContext::get_current_device()->get_vulkan_device(), &ii, nullptr, &image_view);

	cleanup_queue.push_function([=]() { vmaDestroyImage(allocator, new_image.image, new_image.allocation); });

	staging.destroy();
};

} // namespace ForgottenEngine
