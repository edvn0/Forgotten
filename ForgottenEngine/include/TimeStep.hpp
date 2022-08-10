#pragma once

#include <cmath>

namespace ForgottenEngine {

	class TimeStep {
	public:
		using TimeDelta = float;

	public:
		TimeStep(TimeDelta time = 0.0f)
			: time(time) {};
		TimeStep(double time = 0.0f)
			: time(static_cast<float>(time)) {};

		[[nodiscard]] TimeDelta get_seconds() const { return time; };
		[[nodiscard]] TimeDelta get_milli_seconds() const { return time * 1e3f; };
		[[nodiscard]] TimeDelta get_nano_seconds() const { return time * 1e6f; };

		operator float() const { return time; }
		[[nodiscard]] bool is_zero() const { return abs(time) <= 1e-9; }

		TimeStep& operator+=(const TimeStep& rhs)
		{
			this->time += rhs.time;
			return *this;
		}

		TimeStep& operator-=(const TimeStep& rhs)
		{
			this->time -= rhs.time;
			return *this;
		}

	private:
		TimeDelta time;
	};

} // namespace ForgottenEngine
