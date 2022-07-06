//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include "Mesh.hpp"

#include "DeletionQueue.hpp"

#include "VulkanAllocatedBuffer.hpp"
#include "VulkanVertex.hpp"

#include <array>
#include <glm/glm.hpp>
#include <vector>

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
	~VulkanMesh() override = default;

	void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue) override;

	AllocatedBuffer& get_vertex_buffer() override;
	std::vector<Vertex>& get_vertices() override;
	const std::vector<Vertex>& get_vertices() const override;

private:
	DynamicMesh impl;
};

} // ForgottenEngine
