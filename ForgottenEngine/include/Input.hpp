#pragma once

#include "keys/KeyCode.hpp"
#include "keys/MouseCode.hpp"

namespace ForgottenEngine {

class Input {
public:
	static bool key(KeyCode key);
	static bool mouse(MouseCode key);
};

}