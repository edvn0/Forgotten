#include "fg_pch.hpp"

#include "vulkan/VulkanFramebuffer.hpp"

#include "Application.hpp"
#include "render/Renderer.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanImage.hpp"

namespace ForgottenEngine {

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& specification)
		: spec(specification)
	{
		if (specification.width == 0) {
			width = Application::the().get_window().get_width();
			height = Application::the().get_window().get_height();
		} else {
			width = (uint32_t)(specification.width * spec.scale);
			height = (uint32_t)(specification.height * spec.scale);
		}

		// Create all image objects immediately so we can start referencing them
		// elsewhere
		uint32_t attachment_index = 0;
		if (!spec.existing_framebuffer) {
			for (auto& attachment_spec : spec.attachments.texture_attachments) {
				if (spec.existing_image && spec.existing_image->get_specification().Layers > 1) {
					if (Utils::is_depth_format(attachment_spec.format))
						depth_image = spec.existing_image;
					else
						attachment_images.emplace_back(spec.existing_image);
				} else if (spec.existing_images.find(attachment_index) != spec.existing_images.end()) {
					if (!Utils::is_depth_format(attachment_spec.format))
						attachment_images.emplace_back(); // This will be set later
				} else if (Utils::is_depth_format(attachment_spec.format)) {
					ImageSpecification image_specification;
					image_specification.Format = attachment_spec.format;
					image_specification.Usage = ImageUsage::Attachment;
					image_specification.Transfer = image_specification.Transfer;
					image_specification.Width = (uint32_t)(width * spec.scale);
					image_specification.Height = (uint32_t)(height * spec.scale);
					image_specification.DebugName = fmt::format("{0}-DepthAttachment{1}",
						image_specification.DebugName.empty() ? "Unnamed FB" : image_specification.DebugName, attachment_index);
					depth_image = Image2D::create(image_specification);
				} else {
					ImageSpecification image_specification;
					image_specification.Format = attachment_spec.format;
					image_specification.Usage = ImageUsage::Attachment;
					image_specification.Transfer = image_specification.Transfer;
					image_specification.Width = (uint32_t)(width * spec.scale);
					image_specification.Height = (uint32_t)(height * spec.scale);
					image_specification.DebugName = fmt::format("{0}-ColorAttachment{1}",
						image_specification.DebugName.empty() ? "Unnamed FB" : image_specification.DebugName, attachment_index);
					attachment_images.emplace_back(Image2D::create(image_specification));
				}
				attachment_index++;
			}
		}

		core_assert_bool(specification.attachments.texture_attachments.size());
	}

	VulkanFramebuffer::~VulkanFramebuffer() { release(); }

	void VulkanFramebuffer::release()
	{
		if (framebuffer) {
			VkFramebuffer fb = framebuffer;
			Renderer::submit_resource_free([fb]() {
				const auto device = VulkanContext::get_current_device()->get_vulkan_device();
				vkDestroyFramebuffer(device, fb, nullptr);
			});

			// Don't free the images if we don't own them
			if (!spec.existing_framebuffer) {
				uint32_t attachment_index = 0;
				for (Reference<VulkanImage2D> image : attachment_images) {
					if (spec.existing_images.find(attachment_index) != spec.existing_images.end())
						continue;

					// Only destroy deinterleaved image once and prevent clearing layer views on second framebuffer
					// invalidation
					if (image->get_specification().Layers == 1 || (attachment_index == 0 && !image->get_layer_image_view(0)))
						image->release();
					attachment_index++;
				}

				if (depth_image) {
					// Do we own the depth image?
					if (spec.existing_images.find((uint32_t)spec.attachments.texture_attachments.size() - 1) == spec.existing_images.end())
						depth_image->release();
				}
			}
		}
	}

	void VulkanFramebuffer::resize(uint32_t in_width, uint32_t in_height, bool force_recreate)
	{
		if (!force_recreate && (width == in_width && height == in_height))
			return;

		width = (uint32_t)(in_width * spec.scale);
		height = (uint32_t)(in_height * spec.scale);
		if (!spec.swapchain_target) {
			invalidate();
		} else {
			auto& sc = Application::the().get_window().get_swapchain();
			render_pass = sc.get_render_pass();

			clear_values.clear();
			clear_values.emplace_back().color = { 0.1f, 0.1f, 0.1f, 1.0f };
		}

		for (auto& callback : resize_callbacks)
			callback(this);
	}

	void VulkanFramebuffer::add_resize_callback(const std::function<void(Reference<Framebuffer>)>& func) { resize_callbacks.push_back(func); }

	void VulkanFramebuffer::invalidate()
	{
		Reference<VulkanFramebuffer> instance = this;
		Renderer::submit([instance]() mutable { instance->rt_invalidate(); });
	}

	void VulkanFramebuffer::rt_invalidate()
	{
		auto device = VulkanContext::get_current_device()->get_vulkan_device();

		release();

		VulkanAllocator allocator("Framebuffer");

		std::vector<VkAttachmentDescription> attachment_descriptions;

		std::vector<VkAttachmentReference> color_attachment_references;
		VkAttachmentReference depth_attachment_reference;

		clear_values.resize(spec.attachments.texture_attachments.size());

		bool create_images = attachment_images.empty();

		if (spec.existing_framebuffer)
			attachment_images.clear();

		uint32_t attachment_index = 0;
		for (const auto& attachment_spec : spec.attachments.texture_attachments) {
			if (Utils::is_depth_format(attachment_spec.format)) {
				if (spec.existing_image) {
					depth_image = spec.existing_image;
				} else if (spec.existing_framebuffer) {
					Reference<VulkanFramebuffer> existing_framebuffer = spec.existing_framebuffer.as<VulkanFramebuffer>();
					depth_image = existing_framebuffer->get_depth_image();
				} else if (spec.existing_images.find(attachment_index) != spec.existing_images.end()) {
					Reference<Image2D> existing_image = spec.existing_images.at(attachment_index);
					core_assert(
						Utils::is_depth_format(existing_image->get_specification().Format), "Trying to attach non-depth image as depth attachment");
					depth_image = existing_image;
				} else {
					Reference<VulkanImage2D> depth_attachment_image = depth_image.as<VulkanImage2D>();
					auto& depth_spec = depth_attachment_image->get_specification();
					depth_spec.Width = (uint32_t)(width * spec.scale);
					depth_spec.Height = (uint32_t)(height * spec.scale);
					depth_attachment_image->rt_invalidate(); // Create immediately
				}

				VkAttachmentDescription& attachment_description = attachment_descriptions.emplace_back();
				attachment_description.flags = 0;
				attachment_description.format = Utils::vulkan_image_format(attachment_spec.format);
				attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
				attachment_description.loadOp = spec.clear_depth_on_load ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // TODO: if sampling, needs to be store
																			   // (otherwise DONT_CARE is fine)
				attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment_description.initialLayout
					= spec.clear_depth_on_load ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				if (attachment_spec.format == ImageFormat::DEPTH24STENCIL8) // Separate layouts requires a "separate layouts" flag to be enabled
				{
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO: if not sampling
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // TODO: if sampling
					depth_attachment_reference = { attachment_index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
				} else {
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL; // TODO: if not sampling
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL; // TODO: if sampling
					depth_attachment_reference = { attachment_index, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL };
				}
				clear_values[attachment_index].depthStencil = { spec.depth_clear_value, 0 };
			} else {
				// core_assert(!spec.ExistingImage, "Not supported for color attachments");

				Reference<VulkanImage2D> color_attachment;
				if (spec.existing_framebuffer) {
					Reference<VulkanFramebuffer> existing_framebuffer = spec.existing_framebuffer.as<VulkanFramebuffer>();
					Reference<Image2D> existing_image = existing_framebuffer->get_image(attachment_index);
					color_attachment = attachment_images.emplace_back(existing_image).as<VulkanImage2D>();
				} else if (spec.existing_images.find(attachment_index) != spec.existing_images.end()) {
					Reference<Image2D> existing_image = spec.existing_images[attachment_index];
					core_assert(
						!Utils::is_depth_format(existing_image->get_specification().Format), "Trying to attach depth image as color attachment");
					color_attachment = existing_image.as<VulkanImage2D>();
					attachment_images[attachment_index] = existing_image;
				} else {
					if (create_images) {
						ImageSpecification image_specification;
						image_specification.Format = attachment_spec.format;
						image_specification.Usage = ImageUsage::Attachment;
						image_specification.Transfer = image_specification.Transfer;
						image_specification.Width = (uint32_t)(width * spec.scale);
						image_specification.Height = (uint32_t)(height * spec.scale);
						color_attachment = attachment_images.emplace_back(Image2D::create(image_specification)).as<VulkanImage2D>();
					} else {
						Reference<Image2D> image = attachment_images[attachment_index];
						ImageSpecification& image_spec = image->get_specification();
						image_spec.Width = (uint32_t)(width * spec.scale);
						image_spec.Height = (uint32_t)(height * spec.scale);
						color_attachment = image.as<VulkanImage2D>();
						if (color_attachment->get_specification().Layers == 1)
							color_attachment->rt_invalidate(); // Create immediately
						else if (attachment_index == 0
							&& spec.existing_image_layers[0] == 0) // Only invalidate the first layer from only the first framebuffer
						{
							color_attachment->rt_invalidate(); // Create immediately
							color_attachment->rt_create_per_specific_layer_image_views(spec.existing_image_layers);
						} else if (attachment_index == 0) {
							color_attachment->rt_create_per_specific_layer_image_views(spec.existing_image_layers);
						}
					}
				}

				VkAttachmentDescription& attachment_description = attachment_descriptions.emplace_back();
				attachment_description.flags = 0;
				attachment_description.format = Utils::vulkan_image_format(attachment_spec.format);
				attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
				attachment_description.loadOp = spec.clear_colour_on_load ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // TODO: if sampling, needs to be store
																			   // (otherwise DONT_CARE is fine)
				attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment_description.initialLayout
					= spec.clear_colour_on_load ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachment_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				const auto& clear_color = spec.clear_colour;
				clear_values[attachment_index].color = { { clear_color.r, clear_color.g, clear_color.b, clear_color.a } };
				color_attachment_references.emplace_back(VkAttachmentReference { attachment_index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			}

			attachment_index++;
		}

		VkSubpassDescription subpass_description = {};
		subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_description.colorAttachmentCount = uint32_t(color_attachment_references.size());
		subpass_description.pColorAttachments = color_attachment_references.data();
		if (depth_image)
			subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

		// TODO: do we need these?
		// Use subpass dependencies for layout transitions
		std::vector<VkSubpassDependency> dependencies;

		if (!attachment_images.empty()) {
			{
				VkSubpassDependency& depedency = dependencies.emplace_back();
				depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
				depedency.dstSubpass = 0;
				depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				depedency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
			{
				VkSubpassDependency& depedency = dependencies.emplace_back();
				depedency.srcSubpass = 0;
				depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
				depedency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				depedency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
		}

		if (depth_image) {
			{
				VkSubpassDependency& depedency = dependencies.emplace_back();
				depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
				depedency.dstSubpass = 0;
				depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}

			{
				VkSubpassDependency& depedency = dependencies.emplace_back();
				depedency.srcSubpass = 0;
				depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
				depedency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
		}

		// Create the actual renderpass
		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachment_descriptions.size());
		render_pass_info.pAttachments = attachment_descriptions.data();
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass_description;
		render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
		render_pass_info.pDependencies = dependencies.data();

		vk_check(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));

		std::vector<VkImageView> attachments(attachment_images.size());
		for (uint32_t i = 0; i < attachment_images.size(); i++) {
			Reference<VulkanImage2D> image = attachment_images[i].as<VulkanImage2D>();
			if (image->get_specification().Layers > 1)
				attachments[i] = image->get_layer_image_view(spec.existing_image_layers[i]);
			else
				attachments[i] = image->get_image_info().image_view;
			core_assert_bool(attachments[i]);
		}

		if (depth_image) {
			Reference<VulkanImage2D> image = depth_image.as<VulkanImage2D>();
			if (spec.existing_image) {
				core_assert(spec.existing_image_layers.size() == 1, "Depth attachments do not support deinterleaving");
				attachments.emplace_back(image->get_layer_image_view(spec.existing_image_layers[0]));
			} else
				attachments.emplace_back(image->get_image_info().image_view);

			core_assert_bool(attachments.back());
		}

		VkFramebufferCreateInfo framebuffer_create_info = {};
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = uint32_t(attachments.size());
		framebuffer_create_info.pAttachments = attachments.data();
		framebuffer_create_info.width = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;

		vk_check(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffer));
	}

} // namespace ForgottenEngine
