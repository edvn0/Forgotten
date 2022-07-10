//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "Mesh.hpp"

#include "vulkan/VulkanMesh.hpp"

namespace ForgottenEngine {

std::unique_ptr<Mesh> Mesh::create(std::string path) { return std::make_unique<VulkanMesh>(std::move(path)); }

std::unique_ptr<Mesh> Mesh::create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	return std::make_unique<VulkanMesh>(vertices, indices);
}

}