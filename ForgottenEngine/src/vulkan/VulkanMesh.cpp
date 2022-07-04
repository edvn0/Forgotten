//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanMesh.hpp"

namespace ForgottenEngine {

std::vector<Vertex>& DynamicMesh::get_vertices() { return vertices; }

const std::vector<Vertex>& DynamicMesh::get_vertices() const { return vertices; }

AllocatedBuffer& DynamicMesh::get_vertex_buffer() { return vertex_buffer; }

VulkanMesh::VulkanMesh(std::string path) { }

void VulkanMesh::upload() { }

} // ForgottenEngine