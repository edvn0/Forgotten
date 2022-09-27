#pragma once
#include "render/Shader.hpp"

#include <Enumeration.hpp>
#include <shaderc/shaderc.h>
#include <vulkan/vulkan_core.h>

namespace ForgottenEngine::ShaderUtils {

	inline static std::string_view vk_stage_to_shader_macro(const VkShaderStageFlagBits stage)
	{
		switch (stage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "__VERTEX_STAGE__";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "__FRAGMENT_STAGE__";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "__COMPUTE_STAGE__";
		default:
			core_verify(false, "Unknown shader stage.");
		}
	}

	inline static std::string_view stage_to_shader_macro(const std::string_view stage)
	{
		if (stage == "vert")
			return "__VERTEX_STAGE__";
		if (stage == "frag")
			return "__FRAGMENT_STAGE__";
		if (stage == "comp")
			return "__COMPUTE_STAGE__";
		core_verify(false, "Unknown shader stage.");
		return "";
	}

	inline static VkShaderStageFlagBits stage_to_vk_shader_stage(const std::string_view stage)
	{
		if (stage == "vert")
			return VK_SHADER_STAGE_VERTEX_BIT;
		if (stage == "frag")
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		if (stage == "comp")
			return VK_SHADER_STAGE_COMPUTE_BIT;
		core_verify(false, "Unknown shader stage.");
		return VK_SHADER_STAGE_ALL;
	}

	inline static std::string_view shader_stage_to_string(const VkShaderStageFlagBits stage) { return Enumeration::enum_name(stage); }

	inline static VkShaderStageFlagBits shader_type_from_string(const std::string_view type)
	{
		if (type == "vert")
			return VK_SHADER_STAGE_VERTEX_BIT;
		if (type == "frag")
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		if (type == "comp")
			return VK_SHADER_STAGE_COMPUTE_BIT;

		return VK_SHADER_STAGE_ALL;
	}

	inline static SourceLang shader_lang_from_extension(const std::string_view type)
	{
		if (type == ".glsl")
			return SourceLang::GLSL;
		if (type == ".hlsl")
			core_assert(false, "Cannot read HLSL.");

		core_assert_bool(false);

		return SourceLang::NONE;
	}

	inline static shaderc_shader_kind shader_stage_to_shader_c(const VkShaderStageFlagBits stage)
	{
		switch (stage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return shaderc_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return shaderc_fragment_shader;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return shaderc_compute_shader;
		default:
			core_assert_bool(false);
		}
		return {};
	}

	inline static const char* shader_stage_cached_file_extension(const VkShaderStageFlagBits stage, bool debug)
	{
		if (debug) {
			switch (stage) {
			case VK_SHADER_STAGE_VERTEX_BIT:
				return ".cached_vulkan_debug.vert";
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				return ".cached_vulkan_debug.frag";
			case VK_SHADER_STAGE_COMPUTE_BIT:
				return ".cached_vulkan_debug.comp";
			default:
				core_assert_bool(false);
			}
		} else {
			switch (stage) {
			case VK_SHADER_STAGE_VERTEX_BIT:
				return ".cached_vulkan.vert";
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				return ".cached_vulkan.frag";
			case VK_SHADER_STAGE_COMPUTE_BIT:
				return ".cached_vulkan.comp";
			default:
				core_assert_bool(false);
			}
		}

		return "";
	}

} // namespace ForgottenEngine::ShaderUtils
