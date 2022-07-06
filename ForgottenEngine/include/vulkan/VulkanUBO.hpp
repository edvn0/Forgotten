//
// Created by Edwin Carlsson on 2022-07-06.
//

#pragma once

#include <glm/glm.hpp>

namespace ForgottenEngine {

struct CameraUBO {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 v_p;
};

}