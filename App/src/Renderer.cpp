#include "Renderer.hpp"
#include "Random.hpp"

#include <glm/gtx/norm.hpp>

void Renderer::render()
{
	const auto integer_to_double_division = [](const uint32_t a, const uint32_t b) -> const double {
		return static_cast<double>(a) / static_cast<double>(b);
	};

	const auto w = image->get_width();
	const auto h = image->get_height();

	for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			glm::vec2 coord = { integer_to_double_division(x, w), integer_to_double_division(y, h) };
			coord = coord * 2.0f - 1.0f;
			image_data[x + y * w] = per_pixel_draw(coord);
		}
	}

	image->set_data(image_data);
}

uint32_t Renderer::per_pixel_draw(const glm::vec2& coord)
{
	static constexpr glm::vec3 ray_origin(0.0f, 0.0f, -3.0f);
	glm::vec3 ray_direction(coord.x, coord.y, -1.0f);
	static constexpr float radius = 0.5f;
	float a = glm::dot(ray_direction, ray_direction);
	float b = 2.0f * glm::dot(ray_origin, ray_direction);
	float c = glm::dot(ray_origin, ray_origin) - radius * radius;

	float discriminant = b * b - 4.0f * a * c;
	if (discriminant >= 0.0f) {
		return 0xffff00ff;
	}

	return 0xff000000;
}

void Renderer::on_resize(uint32_t width, uint32_t height)
{
	if (image) {
		if (image->get_width() == width && image->get_height() == height) {
			return;
		}

		image->resize(width, height);
	} else {
		image = std::make_shared<Image>(width, height, ImageFormat::RGBA);
	}

	delete[] image_data;
	image_data = new uint32_t[width * height];
}