#pragma once

#include "Image.hpp"

typedef unsigned char stbi_uc;

namespace ForgottenEngine {

class VulkanImage : public Image {
private:
	AllocatedImage image;
	stbi_uc* pixels;

	int w, h, c;

public:
	explicit VulkanImage(std::string path);
	~VulkanImage() override;

	void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context) override;
};
}