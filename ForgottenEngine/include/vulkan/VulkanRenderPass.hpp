//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/RenderPass.hpp"

namespace ForgottenEngine {

class VulkanRenderPass : public RenderPass {
public:
	explicit VulkanRenderPass(const RenderPassSpecification& spec);
	RenderPassSpecification& get_specification() override;
	const RenderPassSpecification& get_specification() const override;
};

}