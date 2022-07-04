//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

// 16 + 12 + 16 = 44
struct Vertex {
	glm::vec4 position;
	glm::vec3 normal;
	glm::vec4 color;

	static VertexInputDescription get_vertex_description();
};

}