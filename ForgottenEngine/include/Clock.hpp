#pragma once

namespace ForgottenEngine {

	class Clock {
	public:
		template <typename T = double>
		static T get_time();
	};

} // namespace ForgottenEngine