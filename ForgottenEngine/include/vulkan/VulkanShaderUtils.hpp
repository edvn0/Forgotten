#pragma once
#include "render/Shader.hpp"

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
			CORE_VERIFY(false, "Unknown shader stage.");
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
		CORE_VERIFY(false, "Unknown shader stage.");
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
		CORE_VERIFY(false, "Unknown shader stage.");
		return VK_SHADER_STAGE_ALL;
	}

	inline static const char* shader_stage_to_string(const VkShaderStageFlagBits stage)
	{
		switch (stage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vert";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "frag";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "comp";
		default:
			CORE_ASSERT_BOOL(false);
		}
		return "UNKNOWN";
	}

	inline static VkShaderStageFlagBits ShaderTypeFromString(const std::string_view type)
	{
		if (type == "vert")
			return VK_SHADER_STAGE_VERTEX_BIT;
		if (type == "frag")
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		if (type == "comp")
			return VK_SHADER_STAGE_COMPUTE_BIT;

		return VK_SHADER_STAGE_ALL;
	}

	inline static SourceLang ShaderLangFromExtension(const std::string_view type)
	{
		if (type == ".glsl")
			return SourceLang::GLSL;
		if (type == ".hlsl")
			CORE_ASSERT(false, "Cannot read HLSL.");

		CORE_ASSERT_BOOL(false);

		return SourceLang::NONE;
	}

	inline static shaderc_shader_kind ShaderStageToShaderC(const VkShaderStageFlagBits stage)
	{
		switch (stage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return shaderc_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return shaderc_fragment_shader;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return shaderc_compute_shader;
		default:
			CORE_ASSERT_BOOL(false);
		}
		CORE_ASSERT_BOOL(false);
		return {};
	}

	inline static const char* ShaderStageCachedFileExtension(const VkShaderStageFlagBits stage, bool debug)
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
				CORE_ASSERT_BOOL(false);
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
				CORE_ASSERT_BOOL(false);
			}
		}

		return "";
	}

	inline static const wchar_t* HLSLShaderProfile(const VkShaderStageFlagBits stage)
	{
		switch (stage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return L"vs_6_0";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return L"ps_6_0";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return L"cs_6_0";
		default:
			CORE_ASSERT_BOOL(false);
		}
		return L"";
	}
} // namespace ForgottenEngine::ShaderUtils
