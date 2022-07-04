//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "Mesh.hpp"

#include "vulkan/VulkanMesh.hpp"

namespace ForgottenEngine {

std::unique_ptr<Mesh> Mesh::create(std::string path) { return std::make_unique<VulkanMesh>(std::move(path)); }

}