//
// Created by Edwin Carlsson on 2022-07-06.
//

#pragma once

#include "VulkanContext.hpp"

#include <glm/glm.hpp>

namespace ForgottenEngine {

	static size_t pad_uniform_buffer_size(size_t original_size)
	{
		// Calculate required alignment based on minimum device offset alignment
		size_t ubo_alignment = 16; // FIXME: Dont do this, call some get_min_alignment
		size_t aligned_size = original_size;
		if (ubo_alignment > 0) {
			aligned_size = (aligned_size + ubo_alignment - 1) & ~(ubo_alignment - 1);
		}
		return aligned_size;
	}

	struct SceneUBO {
		glm::vec4 fog; // w is for exponent
		glm::vec4 fog_distances; // x for min, y for max, zw unused.
		glm::vec4 ambient_color;
		glm::vec4 sunlight_direction; // w for sun power
		glm::vec4 sunlight_color;
	};

	struct CameraUBO {
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 v_p;
	};

} // namespace ForgottenEngine