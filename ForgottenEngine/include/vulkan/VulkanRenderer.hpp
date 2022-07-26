#pragma once

#include "render/RendererAPI.hpp"

namespace ForgottenEngine {

class VulkanRenderer : public RendererAPI {
public:
	~VulkanRenderer() override = default;

	void init() override;
	void shut_down() override;

	void begin_frame() override;
	void end_frame() override;
};

} // namespace ForgottenEngine
