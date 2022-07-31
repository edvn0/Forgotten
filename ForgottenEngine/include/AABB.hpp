#pragma once

#include <glm/glm.hpp>

namespace ForgottenEngine {

struct AABB
{
	glm::vec3 min, max;

	AABB()
		: min(0.0f), max(0.0f) {}

	AABB(const glm::vec3& min, const glm::vec3& max)
		: min(min), max(max) {}

};


}