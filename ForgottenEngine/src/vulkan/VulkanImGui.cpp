#include "fg_pch.hpp"

#include "backends/imgui_impl_vulkan.h"
#include "imgui/CoreUserInterface.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanTexture.hpp"

namespace ImGui {
	extern bool ImageButtonEx(ImGuiID id, ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& padding, const ImVec4& bg_col, const ImVec4& tint_col);
}

namespace ForgottenEngine::UI {

	ImTextureID get_texture_id(const Reference<Texture2D>& texture)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkanTexture = texture.as<VulkanTexture2D>();
			const VkDescriptorImageInfo& imageInfo = vulkanTexture->get_vulkan_descriptor_info();
			if (!imageInfo.imageView)
				CORE_ASSERT(false, "");

			return reinterpret_cast<ImU64>(ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout));
		}

		return (ImTextureID)0;
	}

	void image(const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkanImage = image.as<VulkanImage2D>();
			const auto& imageInfo = vulkanImage->get_image_info();
			if (!imageInfo.image_view)
				return;
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.image_view, vulkanImage->get_descriptor_info().imageLayout);
			ImGui::Image((ImU64)textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void image(const Reference<Image2D>& image, uint32_t imageLayer, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkanImage = image.as<VulkanImage2D>();
			auto imageInfo = vulkanImage->get_image_info();
			imageInfo.image_view = vulkanImage->get_layer_image_view(imageLayer);
			if (!imageInfo.image_view)
				return;
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.image_view, vulkanImage->get_descriptor_info().imageLayout);
			ImGui::Image((ImU64)textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void image_mip(const Reference<Image2D>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		Reference<VulkanImage2D> vulkanImage = image.as<VulkanImage2D>();
		auto imageInfo = vulkanImage->get_image_info();
		imageInfo.image_view = vulkanImage->get_mip_image_view(mip);
		if (!imageInfo.image_view)
			return;

		const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.image_view, vulkanImage->get_descriptor_info().imageLayout);
		ImGui::Image((ImU64)textureID, size, uv0, uv1, tint_col, border_col);
	}

	void image(const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkanTexture = texture.as<VulkanTexture2D>();
			const VkDescriptorImageInfo& imageInfo = vulkanTexture->get_vulkan_descriptor_info();
			if (!imageInfo.imageView)
				return;
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout);
			ImGui::Image((ImU64)textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	bool image_button(const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return image_button(nullptr, image, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool image_button(const char* stringID, const Reference<Image2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanImage2D> vulkanImage = image.as<VulkanImage2D>();
			const auto& imageInfo = vulkanImage->get_image_info();
			if (!imageInfo.image_view)
				return false;
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.image_view, vulkanImage->get_descriptor_info().imageLayout);
			auto id = (ImGuiID)((((uint64_t)imageInfo.image_view) >> 32) ^ (uint32_t)(uint64_t)imageInfo.image_view);
			if (stringID) {
				const ImGuiID strID = ImGui::GetID(stringID);
				id = id ^ strID;
			}
			return ImGui::ImageButtonEx(id, (ImU64)textureID, size, uv0, uv1, ImVec2 { (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
		}

		CORE_VERIFY(false, "Not supported");
		return false;
	}

	bool image_button(const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return image_button(nullptr, texture, size, uv0, uv1, frame_padding, bg_col, tint_col);
	}

	bool image_button(const char* stringID, const Reference<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		CORE_VERIFY(texture, "");
		if (!texture)
			return false;

		if (RendererAPI::current() == RendererAPIType::Vulkan) {
			Reference<VulkanTexture2D> vulkanTexture = texture.as<VulkanTexture2D>();

			// This is technically okay, could mean that GPU just hasn't created the texture yet
			CORE_VERIFY(vulkanTexture->get_image(), "");
			if (!vulkanTexture->get_image())
				return false;

			const VkDescriptorImageInfo& imageInfo = vulkanTexture->get_vulkan_descriptor_info();
			const auto textureID = ImGui_ImplVulkan_AddTexture(imageInfo.sampler, imageInfo.imageView, imageInfo.imageLayout);
			ImGuiID id = (ImGuiID)((((uint64_t)imageInfo.imageView) >> 32) ^ (uint32_t)(uint64_t)imageInfo.imageView);
			if (stringID) {
				const ImGuiID strID = ImGui::GetID(stringID);
				id = id ^ strID;
			}
			return ImGui::ImageButtonEx(id, (ImU64)textureID, size, uv0, uv1, ImVec2 { (float)frame_padding, (float)frame_padding }, bg_col, tint_col);
		}

		CORE_VERIFY(false, "Not supported");
		return false;
	}

} // namespace ForgottenEngine::UI
