#pragma once

#include "Image.hpp"

typedef unsigned char stbi_uc;

namespace ForgottenEngine {

class VulkanImage : public Image {
private:
	AllocatedImage image{ nullptr, nullptr };
	stbi_uc* pixels;

	int w, h, c;

public:
	VkImageView image_view{ nullptr };

public:
	explicit VulkanImage(const std::string& path);
	~VulkanImage() override;

	VkImageView view() override { return image_view; }
	void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context) override;
};
}