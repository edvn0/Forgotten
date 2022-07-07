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
	[[nodiscard]] virtual size_t size() const = 0;
};

class DynamicMesh : public MeshSize {
private:
	std::vector<Vertex> vertices;
	AllocatedBuffer vertex_buffer{ nullptr, nullptr };

public:
	std::vector<Vertex>& get_vertices();
	[[nodiscard]] const std::vector<Vertex>& get_vertices() const;
	AllocatedBuffer& get_vertex_buffer();

	void set_vertices(const std::vector<Vertex>& verts) { vertices = verts; }

	[[nodiscard]] size_t size() const override { return vertices.size() * sizeof(Vertex); }
};

template <size_t T> class FixedSizeMesh : public MeshSize {
private:
	std::array<Vertex, T> vertices;
	AllocatedBuffer vertex_buffer{ nullptr, nullptr };

public:
	[[nodiscard]] size_t size() const override { return T * vertices.size(); };
};

class VulkanMesh : public Mesh {
public:
	explicit VulkanMesh(std::string path);
	explicit VulkanMesh(const std::vector<Vertex>& vertices);
	~VulkanMesh() override = default;

	void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue, UploadContext& upload_context) override;

	AllocatedBuffer& get_vertex_buffer() override;
	std::vector<Vertex>& get_vertices() override;
	const std::vector<Vertex>& get_vertices() const override;

private:
	DynamicMesh mesh;
};

} // ForgottenEngine
