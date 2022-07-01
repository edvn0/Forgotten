#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace Forgotten {

enum class ImageFormat { None = 0, RGBA, RGBA32F };

class Image {
public:
	explicit Image(std::string_view path);
	Image(uint32_t width, uint32_t height, ImageFormat format, const void* data = nullptr);
	~Image();

	void set_data(const void* data);

	[[nodiscard]] VkDescriptorSet get_descriptor_set() const { return descriptor_set; }

	void resize(uint32_t width, uint32_t height);

	[[nodiscard]] uint32_t get_width() const { return width; }
	[[nodiscard]] uint32_t get_height() const { return height; }

private:
	void allocate_memory(uint64_t size);
	void release();

private:
	uint32_t width = 0, height = 0;

	VkImage image = nullptr;
	VkImageView image_view = nullptr;
	VkDeviceMemory memory = nullptr;
	VkSampler sampler = nullptr;

	ImageFormat format = ImageFormat::None;

	VkBuffer staging_buffer = nullptr;
	VkDeviceMemory staging_buffer_memory = nullptr;

	size_t aligned_size = 0;

	VkDescriptorSet descriptor_set = nullptr;

	std::string file_path;
};

}
