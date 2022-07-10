//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include "Mesh.hpp"

#include "Common.hpp"

#include "DeletionQueue.hpp"

#include "VulkanAllocatedBuffer.hpp"
#include "VulkanVertex.hpp"

namespace ForgottenEngine {

struct MeshSize {
	[[nodiscard]] virtual size_t vertices_size() const = 0;
	[[nodiscard]] virtual size_t indices_size() const = 0;
};

class DynamicMesh : public MeshSize {
private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	AllocatedBuffer vertex_buffer{ nullptr, nullptr };
	AllocatedBuffer index_buffer{ nullptr, nullptr };

public:
	std::vector<Vertex>& get_vertices();
	[[nodiscard]] const std::vector<Vertex>& get_vertices() const;
	std::vector<uint32_t>& get_indices();
	[[nodiscard]] const std::vector<uint32_t>& get_indices() const;

	AllocatedBuffer& get_vertex_buffer();
	const AllocatedBuffer& get_vertex_buffer() const;
	AllocatedBuffer& get_index_buffer();
	const AllocatedBuffer& get_index_buffer() const;

	void set_vertices(const std::vector<Vertex>& verts) { vertices = verts; }
	void set_indices(const std::vector<uint32_t>& inds) { indices = inds; }

	[[nodiscard]] size_t vertices_size() const override { return vertices.size() * sizeof(Vertex); }
	[[nodiscard]] size_t indices_size() const override { return indices.size() * sizeof(uint32_t); }
};

template <size_t T, uint32_t Y> class FixedSizeMesh : public MeshSize {
private:
	std::array<Vertex, T> vertices;
	std::array<uint32_t, Y> indices;
	AllocatedBuffer vertex_buffer{ nullptr, nullptr };
	AllocatedBuffer index_buffer{ nullptr, nullptr };

public:
	[[nodiscard]] size_t vertices_size() const override { return T * vertices.size(); };
	[[nodiscard]] size_t indices_size() const override { return Y * indices.size(); };
};

class VulkanMesh : public Mesh {
public:
	explicit VulkanMesh(std::string path);
	explicit VulkanMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	~VulkanMesh() override = default;

	void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context) override;

	AllocatedBuffer& get_vertex_buffer() override;

	[[nodiscard]] const AllocatedBuffer& get_vertex_buffer() const override;

	AllocatedBuffer& get_index_buffer() override;

	const AllocatedBuffer& get_index_buffer() const override;

	std::vector<Vertex>& get_vertices() override;

	[[nodiscard]] const std::vector<Vertex>& get_vertices() const override;

	std::vector<uint32_t>& get_indices() override;

	[[nodiscard]] const std::vector<uint32_t>& get_indices() const override;

private:
	DynamicMesh mesh;
};

} // ForgottenEngine
