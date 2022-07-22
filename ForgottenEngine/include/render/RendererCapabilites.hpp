#pragma once

#include <string>

namespace ForgottenEngine {

struct RendererCapabilities {
	std::string vendor;
	std::string device;
	std::string version;

	int max_samples = 0;
	float max_anisotropy = 0.0f;
	int max_texture_units = 0;
};

}
