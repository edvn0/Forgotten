#pragma once

namespace ForgottenEngine {

class Timer {
public:
	template <typename T = double> static T get_time();
};

}