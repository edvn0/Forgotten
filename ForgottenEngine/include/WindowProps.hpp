#pragma once

#include <string>

namespace ForgottenEngine {

struct WindowProps {
	std::string title;
	uint32_t width;
	uint32_t height;

	WindowProps(const std::string& title = "Engine", uint32_t w = 1280, uint32_t h = 720)
		: title(title)
		, width(w)
		, height(h)
	{
	}
};

}