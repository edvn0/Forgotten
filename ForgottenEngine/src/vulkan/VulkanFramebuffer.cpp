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
		uint32_t attachmentIndex = 0;
		if (!spec.existing_framebuffer) {
			for (auto& attachmentSpec : spec.attachments.texture_attachments) {
				if (spec.existing_image && spec.existing_image->get_specification().Layers > 1) {
					if (Utils::IsDepthFormat(attachmentSpec.format))
						depth_image = spec.existing_image;
					else
						attachment_images.emplace_back(spec.existing_image);
				} else if (spec.existing_images.find(attachmentIndex) != spec.existing_images.end()) {
					if (!Utils::IsDepthFormat(attachmentSpec.format))
						attachment_images.emplace_back(); // This will be set later
				} else if (Utils::IsDepthFormat(attachmentSpec.format)) {
					ImageSpecification image_specification;
					image_specification.Format = attachmentSpec.format;
					image_specification.Usage = ImageUsage::Attachment;
					image_specification.Transfer = image_specification.Transfer;
					image_specification.Width = (uint32_t)(width * spec.scale);
					image_specification.Height = (uint32_t)(height * spec.scale);
					image_specification.DebugName = fmt::format("{0}-DepthAttachment{1}",
						image_specification.DebugName.empty() ? "Unnamed FB" : image_specification.DebugName, attachmentIndex);
					depth_image = Image2D::create(image_specification);
				} else {
					ImageSpecification image_specification;
					image_specification.Format = attachmentSpec.format;
					image_specification.Usage = ImageUsage::Attachment;
					image_specification.Transfer = image_specification.Transfer;
					image_specification.Width = (uint32_t)(width * spec.scale);
					image_specification.Height = (uint32_t)(height * spec.scale);
					image_specification.DebugName = fmt::format("{0}-ColorAttachment{1}",
						image_specification.DebugName.empty() ? "Unnamed FB" : image_specification.DebugName, attachmentIndex);
					attachment_images.emplace_back(Image2D::create(image_specification));
				}
				attachmentIndex++;
			}
		}

		CORE_ASSERT_BOOL(specification.attachments.texture_attachments.size());
		resize(width, height, true);
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
				uint32_t attachmentIndex = 0;
				for (Reference<VulkanImage2D> image : attachment_images) {
					if (spec.existing_images.find(attachmentIndex) != spec.existing_images.end())
						continue;

					// Only destroy deinterleaved image once and prevent clearing layer views on second framebuffer
					// invalidation
					if (image->get_specification().Layers == 1 || (attachmentIndex == 0 && !image->get_layer_image_view(0)))
						image->release();
					attachmentIndex++;
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
			clear_values.emplace_back().color = { 0.0f, 0.0f, 0.0f, 1.0f };
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

		std::vector<VkAttachmentDescription> attachmentDescriptions;

		std::vector<VkAttachmentReference> colorAttachmentReferences;
		VkAttachmentReference depthAttachmentReference;

		clear_values.resize(spec.attachments.texture_attachments.size());

		bool createImages = attachment_images.empty();

		if (spec.existing_framebuffer)
			attachment_images.clear();

		uint32_t attachmentIndex = 0;
		for (auto attachmentSpec : spec.attachments.texture_attachments) {
			if (Utils::IsDepthFormat(attachmentSpec.format)) {
				if (spec.existing_image) {
					depth_image = spec.existing_image;
				} else if (spec.existing_framebuffer) {
					Reference<VulkanFramebuffer> existingFramebuffer = spec.existing_framebuffer.as<VulkanFramebuffer>();
					depth_image = existingFramebuffer->get_depth_image();
				} else if (spec.existing_images.find(attachmentIndex) != spec.existing_images.end()) {
					Reference<Image2D> existingImage = spec.existing_images.at(attachmentIndex);
					CORE_ASSERT(
						Utils::IsDepthFormat(existingImage->get_specification().Format), "Trying to attach non-depth image as depth attachment");
					depth_image = existingImage;
				} else {
					Reference<VulkanImage2D> depthAttachmentImage = depth_image.as<VulkanImage2D>();
					auto& depth_spec = depthAttachmentImage->get_specification();
					depth_spec.Width = (uint32_t)(width * spec.scale);
					depth_spec.Height = (uint32_t)(height * spec.scale);
					depthAttachmentImage->rt_invalidate(); // Create immediately
				}

				VkAttachmentDescription& attachmentDescription = attachmentDescriptions.emplace_back();
				attachmentDescription.flags = 0;
				attachmentDescription.format = Utils::VulkanImageFormat(attachmentSpec.format);
				attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
				attachmentDescription.loadOp = spec.clear_depth_on_load ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // TODO: if sampling, needs to be store
																			  // (otherwise DONT_CARE is fine)
				attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachmentDescription.initialLayout
					= spec.clear_depth_on_load ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				if (attachmentSpec.format == ImageFormat::DEPTH24STENCIL8
					|| true) // Separate layouts requires a "separate layouts" flag to be enabled
				{
					attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO: if not sampling
					attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // TODO: if sampling
					depthAttachmentReference = { attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
				} else {
					attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL; // TODO: if not sampling
					attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL; // TODO: if sampling
					depthAttachmentReference = { attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL };
				}
				clear_values[attachmentIndex].depthStencil = { spec.depth_clear_value, 0 };
			} else {
				// CORE_ASSERT(!spec.ExistingImage, "Not supported for color attachments");

				Reference<VulkanImage2D> colorAttachment;
				if (spec.existing_framebuffer) {
					Reference<VulkanFramebuffer> existingFramebuffer = spec.existing_framebuffer.as<VulkanFramebuffer>();
					Reference<Image2D> existingImage = existingFramebuffer->get_image(attachmentIndex);
					colorAttachment = attachment_images.emplace_back(existingImage).as<VulkanImage2D>();
				} else if (spec.existing_images.find(attachmentIndex) != spec.existing_images.end()) {
					Reference<Image2D> existingImage = spec.existing_images[attachmentIndex];
					CORE_ASSERT(!Utils::IsDepthFormat(existingImage->get_specification().Format), "Trying to attach depth image as color attachment");
					colorAttachment = existingImage.as<VulkanImage2D>();
					attachment_images[attachmentIndex] = existingImage;
				} else {
					if (createImages) {
						ImageSpecification image_specification;
						image_specification.Format = attachmentSpec.format;
						image_specification.Usage = ImageUsage::Attachment;
						image_specification.Transfer = image_specification.Transfer;
						image_specification.Width = (uint32_t)(width * spec.scale);
						image_specification.Height = (uint32_t)(height * spec.scale);
						colorAttachment = attachment_images.emplace_back(Image2D::create(image_specification)).as<VulkanImage2D>();
					} else {
						Reference<Image2D> image = attachment_images[attachmentIndex];
						ImageSpecification& image_spec = image->get_specification();
						image_spec.Width = (uint32_t)(width * spec.scale);
						image_spec.Height = (uint32_t)(height * spec.scale);
						colorAttachment = image.as<VulkanImage2D>();
						if (colorAttachment->get_specification().Layers == 1)
							colorAttachment->rt_invalidate(); // Create immediately
						else if (attachmentIndex == 0
							&& spec.existing_image_layers[0] == 0) // Only invalidate the first layer from only the first framebuffer
						{
							colorAttachment->rt_invalidate(); // Create immediately
							colorAttachment->rt_create_per_specific_layer_image_views(spec.existing_image_layers);
						} else if (attachmentIndex == 0) {
							colorAttachment->rt_create_per_specific_layer_image_views(spec.existing_image_layers);
						}
					}
				}

				VkAttachmentDescription& attachmentDescription = attachmentDescriptions.emplace_back();
				attachmentDescription.flags = 0;
				attachmentDescription.format = Utils::VulkanImageFormat(attachmentSpec.format);
				attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
				attachmentDescription.loadOp = spec.clear_colour_on_load ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // TODO: if sampling, needs to be store
																			  // (otherwise DONT_CARE is fine)
				attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachmentDescription.initialLayout
					= spec.clear_colour_on_load ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				const auto& clearColor = spec.clear_colour;
				clear_values[attachmentIndex].color = { { clearColor.r, clearColor.g, clearColor.b, clearColor.a } };
				colorAttachmentReferences.emplace_back(VkAttachmentReference { attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			}

			attachmentIndex++;
		}

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = uint32_t(colorAttachmentReferences.size());
		subpassDescription.pColorAttachments = colorAttachmentReferences.data();
		if (depth_image)
			subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

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
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass));

		std::vector<VkImageView> attachments(attachment_images.size());
		for (uint32_t i = 0; i < attachment_images.size(); i++) {
			Reference<VulkanImage2D> image = attachment_images[i].as<VulkanImage2D>();
			if (image->get_specification().Layers > 1)
				attachments[i] = image->get_layer_image_view(spec.existing_image_layers[i]);
			else
				attachments[i] = image->get_image_info().image_view;
			CORE_ASSERT_BOOL(attachments[i]);
		}

		if (depth_image) {
			Reference<VulkanImage2D> image = depth_image.as<VulkanImage2D>();
			if (spec.existing_image) {
				CORE_ASSERT(spec.existing_image_layers.size() == 1, "Depth attachments do not support deinterleaving");
				attachments.emplace_back(image->get_layer_image_view(spec.existing_image_layers[0]));
			} else
				attachments.emplace_back(image->get_image_info().image_view);

			CORE_ASSERT_BOOL(attachments.back());
		}

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = render_pass;
		framebufferCreateInfo.attachmentCount = uint32_t(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = height;
		framebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer));
	}

} // namespace ForgottenEngine
