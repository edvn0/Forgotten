//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "fg_pch.hpp"

#include "Logger.hpp"

#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanShader.hpp"

#include "Assets.hpp"

namespace ForgottenEngine {

static const std::filesystem::path RESOURCE_PATH = "resources";

VulkanShader::VulkanShader(std::string path)
{
	auto cp = path;
	if (!load_shader_from_file(std::move(path))) {
		CORE_ERROR("{}", "Could not read shader.");
	} else {
		CORE_INFO("Shader at {} read.", cp);
	}
}

VkShaderModule VulkanShader::get_module() { return shader; }

void VulkanShader::destroy()
{
	vkDestroyShaderModule(VulkanContext::get_current_device()->get_vulkan_device(), shader, nullptr);
}

bool load_vulkan_shader_module(const std::string& path, VkShaderModule* out_module)
{
	// open the file. With cursor at the end
	auto p = Assets::load(path);

	if (!p) {
		CORE_ASSERT(false, "Could not find file.");
	}

	auto& file = *p;

	// find what the size of the file is by looking up the location of the cursor
	// because the cursor is at the end, it gives the size directly in bytes
	auto file_size = static_cast<size_t>(file.tellg());

	// spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire
	// file
	std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

	// put file cursor at beginning
	file.seekg(0);

	// load the entire file into the buffer
	file.read((char*)buffer.data(), static_cast<long>(file_size));

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;

	create_info.codeSize = buffer.size() * sizeof(uint32_t);
	create_info.pCode = buffer.data();

	// check that the creation goes well.
	VkShaderModule shader_module;
	VK_CHECK(vkCreateShaderModule(
		VulkanContext::get_current_device()->get_vulkan_device(), &create_info, nullptr, &shader_module));

	*out_module = shader_module;

	return true;
}

}
