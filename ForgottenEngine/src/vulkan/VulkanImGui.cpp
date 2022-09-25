#include "fg_pch.hpp"

#include "backends/imgui_impl_vulkan.h"
#include "imgui/CoreUserInterface.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanTexture.hpp"

namespace ImGui {
	extern bool ImageButtonEx(ImGuiID id, ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& padding,
		const ImVec4& bg_col, const ImVec4& tint_col);
}

namespace ForgottenEngine::UI {

	ImTextureID get_texture_id(const Reference<Texture2D>& texture)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkan_texture = texture.as<VulkanTexture2D>();
			const VkDescriptorImageInfo& image_info = vulkan_texture->get_vulkan_descriptor_info();
			if (!image_info.imageView)
				core_assert_bool(false);

			return reinterpret_cast<ImU64>(ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.imageView, image_info.imageLayout));
		}

		return (ImTextureID)0;
	}

	ImTextureID get_texture_id(const Reference<Image2D>& image, VkImageLayout layout)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkan_texture = image.as<VulkanImage2D>();
			const auto& image_info = vulkan_texture->get_image_info();
			if (!image_info.image_view)
				core_assert_bool(false);

			return reinterpret_cast<ImU64>(ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.image_view, layout));
		}

		return (ImTextureID)0;
	}

	void image(
		const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkan_image = image.as<VulkanImage2D>();
			const auto& image_info = vulkan_image->get_image_info();
			if (!image_info.image_view)
				return;
			const auto texture_id = get_texture_id(image, vulkan_image->get_descriptor_info().imageLayout);
			ImGui::Image(texture_id, size, uv0, uv1, tint_col, border_col);
		}
	}

	void image(const Reference<Image2D>& image, uint32_t image_layer, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1,
		const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkan_image = image.as<VulkanImage2D>();
			auto& image_info = vulkan_image->get_image_info();
			image_info.image_view = vulkan_image->get_layer_image_view(image_layer);
			if (!image_info.image_view)
				return;
			const auto texture_id
				= ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.image_view, vulkan_image->get_descriptor_info().imageLayout);
			ImGui::Image((ImU64)texture_id, size, uv0, uv1, tint_col, border_col);
		}
	}

	void image_mip(const Reference<Image2D>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col,
		const ImVec4& border_col)
	{
		Reference<VulkanImage2D> vulkan_image = image.as<VulkanImage2D>();
		auto& image_info = vulkan_image->get_image_info();
		image_info.image_view = vulkan_image->get_mip_image_view(mip);
		if (!image_info.image_view)
			return;

		const auto texture_id
			= ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.image_view, vulkan_image->get_descriptor_info().imageLayout);
		ImGui::Image((ImU64)texture_id, size, uv0, uv1, tint_col, border_col);
	}

	void image(const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col,
		const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkan_texture = texture.as<VulkanTexture2D>();
			const VkDescriptorImageInfo& image_info = vulkan_texture->get_vulkan_descriptor_info();
			if (!image_info.imageView)
				return;
			const auto texture_id = ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.imageView, image_info.imageLayout);
			ImGui::Image((ImU64)texture_id, size, uv0, uv1, tint_col, border_col);
		}
	}

	bool image_button(const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding,
		const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return image_button(nullptr, image, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool image_button(const char* string_id, const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1,
		int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkan_image = image.as<VulkanImage2D>();
			const auto& image_info = vulkan_image->get_image_info();
			if (!image_info.image_view)
				return false;
			const auto texture_id
				= ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.image_view, vulkan_image->get_descriptor_info().imageLayout);
			auto id = (ImGuiID)((((uint64_t)image_info.image_view) >> 32) ^ (uint32_t)(uint64_t)image_info.image_view);
			if (string_id) {
				const ImGuiID str_id = ImGui::GetID(string_id);
				id = id ^ str_id;
			}
			return ImGui::ImageButtonEx(
				id, (ImU64)texture_id, size, uv0, uv1, ImVec2 { (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
		}

		core_verify(false, "Not supported");
		return false;
	}

	bool image_button(const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const int frame_padding,
		const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return image_button(nullptr, texture, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool image_button(const char* string_id, const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1,
		int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		core_verify_bool(texture);
		if (!texture)
			return false;

		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkan_texture = texture.as<VulkanTexture2D>();

			// This is technically okay, could mean that GPU just hasn't created the texture yet
			core_verify_bool(vulkan_texture->get_image());
			if (!vulkan_texture->get_image())
				return false;

			const VkDescriptorImageInfo& image_info = vulkan_texture->get_vulkan_descriptor_info();
			const auto texture_id = ImGui_ImplVulkan_AddTexture(image_info.sampler, image_info.imageView, image_info.imageLayout);
			ImGuiID id = (ImGuiID)((((uint64_t)image_info.imageView) >> 32) ^ (uint32_t)(uint64_t)image_info.imageView);
			if (string_id) {
				const ImGuiID str_id = ImGui::GetID(string_id);
				id = id ^ str_id;
			}
			return ImGui::ImageButtonEx(
				id, (ImU64)texture_id, size, uv0, uv1, ImVec2 { (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
		}

		core_verify(false, "Not supported");
		return false;
	}

} // namespace ForgottenEngine::UI
