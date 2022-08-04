//
// Created by Edwin Carlsson on 2022-08-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanRenderPass.hpp"

namespace ForgottenEngine {

VulkanRenderPass::VulkanRenderPass(const RenderPassSpecification& spec)
	: spec(spec)
{
}
RenderPassSpecification& VulkanRenderPass::get_specification() { return spec; }
const RenderPassSpecification& VulkanRenderPass::get_specification() const { return spec; }
}