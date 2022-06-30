#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace Forgotten {

class Timer {
public:
	Timer() { reset(); }

	void reset() { start_time = std::chrono::high_resolution_clock::now(); }

	float elapsed()
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
				   std::chrono::high_resolution_clock::now() - start_time)
				   .count()
			* 0.001f * 0.001f * 0.001f;
	}

	float elapsed_millis() { return elapsed() * 1000.0f; }

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};

class ScopedTimer {
public:
	explicit ScopedTimer(std::string name)
		: name(std::move(name))
	{
	}
	~ScopedTimer()
	{
		float time = timer.elapsed_millis();
		std::cout << "[TIMER] " << name << " - " << time << "ms\n";
	}

private:
	std::string name;
	Timer timer;
};

}