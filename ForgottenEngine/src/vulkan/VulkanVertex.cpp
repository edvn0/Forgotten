//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanVertex.hpp"

namespace ForgottenEngine {

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description = {};

	VkVertexInputBindingDescription main_binding = {};
	main_binding.binding = 0;
	main_binding.stride = sizeof(Vertex);
	main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(main_binding);

	VkVertexInputAttributeDescription position_attribute = {};
	position_attribute.binding = 0;
	position_attribute.location = 0;
	position_attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	position_attribute.offset = offsetof(Vertex, position);

	// Normal will be stored at Location 1
	VkVertexInputAttributeDescription normal_attribute = {};
	normal_attribute.binding = 0;
	normal_attribute.location = 1;
	normal_attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	normal_attribute.offset = offsetof(Vertex, normal);

	// Color will be stored at Location 2
	VkVertexInputAttributeDescription color_attribute = {};
	color_attribute.binding = 0;
	color_attribute.location = 2;
	color_attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	color_attribute.offset = offsetof(Vertex, color);

	VkVertexInputAttributeDescription uv_attribute = {};
	uv_attribute.binding = 0;
	uv_attribute.location = 3;
	uv_attribute.format = VK_FORMAT_R32G32_SFLOAT;
	uv_attribute.offset = offsetof(Vertex, uv);

	description.attributes = { position_attribute, normal_attribute, color_attribute, uv_attribute };
	return description;
}

}