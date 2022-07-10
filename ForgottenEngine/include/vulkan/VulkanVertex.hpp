//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
	glm::vec4 position;
	glm::vec4 normal;
	glm::vec4 color;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();

	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && uv == other.uv && normal == other.normal;
	}
};

}

namespace std {
template <> struct hash<ForgottenEngine::Vertex> {
	size_t operator()(ForgottenEngine::Vertex const& vertex) const
	{
		return ((hash<glm::vec4>()(vertex.position)
					^ (hash<glm::vec4>()(vertex.normal) ^ (hash<glm::vec4>()(vertex.color) << 1)) >> 1)
			^ (hash<glm::vec2>()(vertex.uv) << 1));
	}
};
}