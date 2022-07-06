//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanMesh.hpp"

#include "DeletionQueue.hpp"
#include "tiny_obj_loader.h"

namespace ForgottenEngine {

std::vector<Vertex>& DynamicMesh::get_vertices() { return vertices; }

const std::vector<Vertex>& DynamicMesh::get_vertices() const { return vertices; }

AllocatedBuffer& DynamicMesh::get_vertex_buffer() { return vertex_buffer; }

VulkanMesh::VulkanMesh(std::string path)
{
	std::filesystem::path fp{ path };
	// attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	// shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	// materials contains the information about the material of each shape, but we won't use it.
	std::vector<tinyobj::material_t> materials;

	// error and warning output from the load function
	std::string warn;
	std::string err;

	// load the OBJ file
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fp.c_str(), fp.parent_path().c_str());
	// make sure to output the warnings to the console, in case there are issues with the file
	if (!warn.empty()) {
		CORE_WARN("WARN: {}", warn);
	}
	// if we have any error, print it to the console, and break the mesh loading.
	// This happens if the file can't be found or is malformed
	if (!err.empty()) {
		CORE_ERROR("Error in loading Mesh - {}: {}", path, err);
	}

	auto& vertices = impl.get_vertices();
	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			// hardcode loading to triangles
			int fv = 3;
			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				// vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				// vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				// copy it into our vertex
				Vertex new_vert;
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;
				new_vert.position.w = 1.0f;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;
				new_vert.normal.w = 1.0f;

				// we are setting the vertex color as the vertex normal. This is just for display purposes
				new_vert.color = new_vert.normal;

				vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
	}
}

AllocatedBuffer& VulkanMesh::get_vertex_buffer() { return impl.get_vertex_buffer(); }

std::vector<Vertex>& VulkanMesh::get_vertices() { return impl.get_vertices(); }

const std::vector<Vertex>& VulkanMesh::get_vertices() const { return impl.get_vertices(); }

void VulkanMesh::upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue)
{
	// allocate vertex buffer
	VkBufferCreateInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// this is the total size, in bytes, of the buffer we are allocating
	bi.size = impl.size();
	// this buffer is going to be used as a Vertex Buffer
	bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	// let the VMA library know that this data should be writeable by CPU, but also readable by GPU
	VmaAllocationCreateInfo ai = {};
	ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	// allocate the buffer
	auto& vb = impl.get_vertex_buffer();
	VK_CHECK(vmaCreateBuffer(allocator, &bi, &ai, &vb.buffer, &vb.allocation, nullptr));

	// add the destruction of triangle mesh buffer to the deletion queue
	cleanup_queue.push_function([=]() { vmaDestroyBuffer(allocator, vb.buffer, vb.allocation); });

	void* data;
	vmaMapMemory(allocator, vb.allocation, &data);

	memcpy(data, impl.get_vertices().data(), impl.size());

	vmaUnmapMemory(allocator, vb.allocation);
}

} // ForgottenEngine
