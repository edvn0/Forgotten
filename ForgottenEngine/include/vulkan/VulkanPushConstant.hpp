//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

// add the include for glm to get matrices
#include <glm/glm.hpp>

namespace ForgottenEngine {

	struct MeshPushConstants {
		glm::vec4 data;
		glm::mat4 render_matrix;
	};

} // namespace ForgottenEngine