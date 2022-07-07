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

struct Vertex {
	glm::vec4 position;
	glm::vec4 normal;
	glm::vec4 color;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();
};

}