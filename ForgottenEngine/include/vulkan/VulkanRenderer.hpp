#pragma once

#include "render/RendererAPI.hpp"

namespace ForgottenEngine {

class VulkanRenderer : public RendererAPI {
public:
	virtual void init() override;
	virtual void shut_down() override;

	virtual void begin_frame() override;
	virtual void end_frame() override;
};

} // namespace ForgottenEngine
