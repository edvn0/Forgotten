#pragma once

#include "codes/KeyCode.hpp"
#include "codes/MouseCode.hpp"

#include <glm/glm.hpp>

namespace ForgottenEngine {

	class Input {
	public:
		static bool key(KeyCode key);

		static bool mouse(MouseCode key);

		static glm::vec2 mouse_position();
	};

} // namespace ForgottenEngine
