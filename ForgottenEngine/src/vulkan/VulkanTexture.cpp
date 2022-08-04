//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanTexture.hpp"

namespace ForgottenEngine {

VulkanTexture2D::VulkanTexture2D(const std::string& path, TextureProperties properties) { }
VulkanTexture2D::VulkanTexture2D(
	ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties)
{
}
VulkanTexture2D::~VulkanTexture2D() { }
void VulkanTexture2D::resize(const glm::uvec2& size) { }
void VulkanTexture2D::resize(uint32_t width, uint32_t height) { }

void VulkanTexture2D::lock() { }
void VulkanTexture2D::unlock() { }
Buffer VulkanTexture2D::get_writeable_buffer() { }
bool VulkanTexture2D::is_loaded() const { }
const std::string& VulkanTexture2D::get_path() const { }

void VulkanTexture2D::bind_impl(uint32_t slot) const { }

uint32_t VulkanTexture2D::get_mip_level_count() const { }

std::pair<uint32_t, uint32_t> VulkanTexture2D::get_mip_size(uint32_t mip) const { }

void VulkanTexture2D::generate_mips() { }

bool VulkanTexture2D::load_image(const std::string& path) { }

bool VulkanTexture2D::load_image(const void* data, uint32_t size) { }

VulkanTextureCube::VulkanTextureCube(
	ImageFormat format, uint32_t width, uint32_t height, const void* data, TextureProperties properties)
{
}

VulkanTextureCube::VulkanTextureCube(const std::string& path, TextureProperties properties) { }

void VulkanTextureCube::release() { }

VulkanTextureCube::~VulkanTextureCube() { }

void VulkanTextureCube::bind_impl(uint32_t slot) const { }

std::pair<uint32_t, uint32_t> VulkanTextureCube::get_mip_size(uint32_t mip) const { }

uint64_t VulkanTextureCube::get_hash() const { }

VkImageView VulkanTextureCube::create_image_view_single_mip(uint32_t mip) { }

void VulkanTextureCube::generate_mips(bool readonly) { }

void VulkanTextureCube::invalidate() { }

}