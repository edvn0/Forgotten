//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include "Common.hpp"

#include "vulkan/VulkanAllocatedBuffer.hpp"
#include "vulkan/VulkanVertex.hpp"

#include "DeletionQueue.hpp"

namespace ForgottenEngine {

class Mesh {
public:
	virtual ~Mesh() = default;
	virtual void upload(VmaAllocator& allocator, DeletionQueue& cleanup_queue) = 0;

	virtual std::vector<Vertex>& get_vertices() = 0;
	[[nodiscard]] virtual const std::vector<Vertex>& get_vertices() const = 0;
	virtual AllocatedBuffer& get_vertex_buffer() = 0;

public:
	static std::unique_ptr<Mesh> create(std::string path);
};

}
