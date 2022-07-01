#pragma once

#include <random>

#include <glm/glm.hpp>

namespace Forgotten {

class Random {
public:
	static void init() { random_engine.seed(std::random_device()()); }

	static uint32_t uint() { return random_distribution(random_engine); }

	static uint32_t uint(uint32_t min, uint32_t max)
	{
		return min + (random_distribution(random_engine) % (max - min + 1));
	}

	static float random_float()
	{
		return (float)random_distribution(random_engine) / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 random_vec3() { return { random_float(), random_float(), random_float() }; }

	static glm::vec3 random_vec3(float min, float max)
	{
		return { random_float() * (max - min) + min, random_float() * (max - min) + min,
			random_float() * (max - min) + min };
	}

	static glm::vec3 random_in_unit_sphere() { return glm::normalize(random_vec3(-1.0f, 1.0f)); }

private:
	static std::mt19937 random_engine;
	static std::uniform_int_distribution<std::mt19937::result_type> random_distribution;
};

}
