//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "fg_pch.hpp"

#include "Shader.hpp"

#include "vulkan/VulkanShader.hpp"

namespace ForgottenEngine {

std::unique_ptr<Shader> Shader::create(std::string path)
{
	return std::make_unique<VulkanShader>(std::move(path));
}

}