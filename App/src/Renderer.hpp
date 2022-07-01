#pragma once

#include "Image.hpp"
#include <glm/glm.hpp>
#include <imgui.h>

using namespace Forgotten;

class Renderer {
public:
	Renderer() = default;

	void render();
	void on_resize(uint32_t width, uint32_t height);

	[[nodiscard]] const auto& get_final_image() const { return image; }

	uint32_t per_pixel_draw(const glm::vec2& coord);

private:
	std::shared_ptr<Image> image;
	uint32_t* image_data{ nullptr };
};