#pragma once

#include "Common.hpp"

#include "vulkan/VulkanAllocatedBuffer.hpp"
#include "vulkan/VulkanUploadContext.hpp"
#include "vulkan/VulkanVertex.hpp"

#include "DeletionQueue.hpp"

namespace ForgottenEngine {

class Image {
public:
	virtual ~Image() = default;
	virtual void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context) = 0;

public:
	static std::unique_ptr<Image> create(std::string path);
};

}