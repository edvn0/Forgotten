#pragma once

#include "codes/KeyCode.hpp"
#include "codes/MouseCode.hpp"

namespace ForgottenEngine {

	class Input {
	public:
		static bool key(KeyCode key);
		static bool mouse(MouseCode key);
	};

} // namespace ForgottenEngine