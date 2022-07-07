//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "Image.hpp"

#include "vulkan/VulkanImage.hpp"

namespace ForgottenEngine {

std::unique_ptr<Image> Image::create(std::string path) { return std::make_unique<VulkanImage>(std::move(path)); }

}