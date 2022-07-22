#pragma once

#include <string>
#include <utility>

namespace ForgottenEngine {

struct RendererConfig {
	uint32_t frames_in_flight = 3;

	bool compute_environment_maps = true;

	// Tiering settings
	uint32_t environment_map_resolution = 1024;
	uint32_t irradiance_map_compute_samples = 512;

	std::string shader_pack_path = "shaders";
};

struct ApplicationProperties {
	std::string title;
	uint32_t width;
	uint32_t height;
	bool fullscreen = false;
	bool vsync = false;
	RendererConfig renderer_config;

	explicit ApplicationProperties(std::string title = "Engine", uint32_t w = 1280, uint32_t h = 720,
		bool fullscreen = false, bool vsync = false, RendererConfig renderer_config = {})
		: title(std::move(title))
		, width(w)
		, height(h)
		, fullscreen(fullscreen)
		, vsync(vsync)
		, renderer_config(std::move(renderer_config))
	{
	}
};

}