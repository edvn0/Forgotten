//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "DeletionQueue.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanMesh.hpp"

#include "tiny_obj_loader.h"

namespace ForgottenEngine {

std::vector<Vertex>& DynamicMesh::get_vertices() { return vertices; }

const std::vector<Vertex>& DynamicMesh::get_vertices() const { return vertices; }

AllocatedBuffer& DynamicMesh::get_vertex_buffer() { return vertex_buffer; }

const AllocatedBuffer& DynamicMesh::get_vertex_buffer() const { return vertex_buffer; }

AllocatedBuffer& DynamicMesh::get_index_buffer() { return index_buffer; }

const AllocatedBuffer& DynamicMesh::get_index_buffer() const { return index_buffer; }

std::vector<uint32_t>& DynamicMesh::get_indices() { return indices; }

const std::vector<uint32_t>& DynamicMesh::get_indices() const { return indices; }

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

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fp.c_str(), fp.parent_path().c_str());
	if (!warn.empty()) {
		CORE_WARN("WARN: {}", warn);
	}
	// if we have any error, print it to the console, and break the mesh loading.
	// This happens if the file can't be found or is malformed
	if (!err.empty()) {
		CORE_ERROR("Error in loading Mesh - {}: {}", path, err);
	}

	std::unordered_map<Vertex, size_t> uniques{};

	auto& vertices = mesh.get_vertices();
	auto& indices = mesh.get_indices();
	// Loop over shapes
	for (auto& shape : shapes) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
			// hardcode loading to triangles
			int fv = 3;
			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

				// vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				// vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				// copy it into our vertex
				Vertex new_vert{};
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

				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				new_vert.uv.x = ux;
				new_vert.uv.y = 1 - uy;

				if (uniques.count(new_vert) == 0) {
					uniques[new_vert] = vertices.size();
					vertices.push_back(new_vert);
				}

				indices.push_back(uniques[new_vert]);
			}
			index_offset += fv;
		}
	}
}

VulkanMesh::VulkanMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	mesh.set_vertices(vertices);
	mesh.set_indices(indices);
}

AllocatedBuffer& VulkanMesh::get_vertex_buffer() { return mesh.get_vertex_buffer(); }

const AllocatedBuffer& VulkanMesh::get_vertex_buffer() const { return mesh.get_vertex_buffer(); }

AllocatedBuffer& VulkanMesh::get_index_buffer() { return mesh.get_index_buffer(); }

const AllocatedBuffer& VulkanMesh::get_index_buffer() const { return mesh.get_index_buffer(); }

std::vector<Vertex>& VulkanMesh::get_vertices() { return mesh.get_vertices(); }

const std::vector<Vertex>& VulkanMesh::get_vertices() const { return mesh.get_vertices(); }

std::vector<uint32_t>& VulkanMesh::get_indices() { return mesh.get_indices(); }

const std::vector<uint32_t>& VulkanMesh::get_indices() const { return mesh.get_indices(); }

void VulkanMesh::upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context)
{
	{
		VulkanStagingBuffer staging_buffer{ allocator, mesh.vertices_size() };
		staging_buffer.set_data(mesh.get_vertices().data(), mesh.vertices_size());

		VkBufferCreateInfo vbi = {};
		vbi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vbi.pNext = nullptr;
		// this is the total size, in bytes, of the buffer we are allocating
		vbi.size = mesh.vertices_size();
		// this buffer is going to be used as a Vertex Buffer
		vbi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		// let the VMA library know that this data should be GPU native
		VmaAllocationCreateInfo ai{};
		ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		// allocate the buffer
		VK_CHECK(vmaCreateBuffer(allocator, &vbi, &ai, &mesh.get_vertex_buffer().buffer,
			&mesh.get_vertex_buffer().allocation, nullptr));
		auto& buffer = staging_buffer.get_buffer();
		upload_context.immediate_submit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = mesh.vertices_size();
			vkCmdCopyBuffer(cmd, buffer.buffer, mesh.get_vertex_buffer().buffer, 1, &copy);
		});
		staging_buffer.destroy();
	}

	{
		VulkanStagingBuffer staging_buffer_index{ allocator, mesh.indices_size() };
		staging_buffer_index.set_data(mesh.get_indices().data(), mesh.indices_size());

		VkBufferCreateInfo vbi_index = {};
		vbi_index.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vbi_index.pNext = nullptr;
		// this is the total size, in bytes, of the buffer we are allocating
		vbi_index.size = mesh.indices_size();
		// this buffer is going to be used as a Vertex Buffer
		vbi_index.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		// let the VMA library know that this data should be GPU native
		VmaAllocationCreateInfo ai_index{};
		ai_index.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		// allocate the buffer
		VK_CHECK(vmaCreateBuffer(allocator, &vbi_index, &ai_index, &mesh.get_index_buffer().buffer,
			&mesh.get_index_buffer().allocation, nullptr));
		auto& index_buffer = staging_buffer_index.get_buffer();
		upload_context.immediate_submit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = mesh.indices_size();
			vkCmdCopyBuffer(cmd, index_buffer.buffer, mesh.get_index_buffer().buffer, 1, &copy);
		});
		staging_buffer_index.destroy();
	}

	cleanup_queue.push_function([=]() {
		vmaDestroyBuffer(allocator, mesh.get_vertex_buffer().buffer, mesh.get_vertex_buffer().allocation);
		vmaDestroyBuffer(allocator, mesh.get_index_buffer().buffer, mesh.get_index_buffer().allocation);
	});
}
// ForgottenEngine

}
