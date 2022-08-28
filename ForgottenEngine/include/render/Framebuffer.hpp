#pragma once

#include "Common.hpp"
#include "Image.hpp"

#include <glm/glm.hpp>
#include <map>

namespace ForgottenEngine {

	class Framebuffer;

	enum class FramebufferBlendMode { None = 0, OneZero, SrcAlphaOneMinusSrcAlpha, Additive, Zero_SrcColor };

	struct FramebufferTextureSpecification {
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(ImageFormat format)
			: format(format)
		{
		}

		ImageFormat format { ImageFormat::None };
		bool blend = true;
		FramebufferBlendMode blend_mode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
		// TODO: filtering/wrap
	};

	struct FramebufferAttachmentSpecification {
		using FBASpec = std::initializer_list<FramebufferTextureSpecification>;

		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(const FBASpec& attachments)
			: texture_attachments(attachments)
		{
		}

		std::vector<FramebufferTextureSpecification> texture_attachments;
	};

	struct FramebufferSpecification {
		float scale = 1.0f;
		uint32_t width = 0;
		uint32_t height = 0;
		glm::vec4 clear_colour = { 0.0f, 0.0f, 0.0f, 1.0f };
		float depth_clear_value = 0.0f;
		bool clear_colour_on_load = true;
		bool clear_depth_on_load = true;

		FramebufferAttachmentSpecification attachments;
		uint32_t samples = 1; // multisampling

		// TODO: Temp, needs scale
		bool no_resize = false;

		// Master switch (individual attachments can be disabled in FramebufferTextureSpecification)
		bool blend = true;
		// None means use BlendMode in FramebufferTextureSpecification
		FramebufferBlendMode blend_mode = FramebufferBlendMode::None;

		// SwapChainTarget = screen buffer (i.e. no framebuffer)
		bool swapchain_target = false;

		// Note: these are used to attach multi-layered color/depth images
		Reference<Image2D> existing_image;
		std::vector<uint32_t> existing_image_layers;

		// Specify existing images to attach instead of creating
		// new images. attachment index -> image
		std::map<uint32_t, Reference<Image2D>> existing_images;

		// At the moment this will just create a new render pass
		// with an existing framebuffer
		Reference<Framebuffer> existing_framebuffer;

		std::string debug_name;
		bool transfer;
	};

	class Framebuffer : public ReferenceCounted {
	public:
		virtual ~Framebuffer() { }
		virtual void bind() const = 0;
		virtual void unbind() const = 0;

		virtual void resize(uint32_t width, uint32_t height, bool forceRecreate) = 0;
		virtual void add_resize_callback(const std::function<void(Reference<Framebuffer>)>& func) = 0;

		virtual void bind_texture(uint32_t attachmentIndex, uint32_t slot) const = 0;

		virtual uint32_t get_width() const = 0;
		virtual uint32_t get_height() const = 0;

		virtual RendererID get_renderer_id() const = 0;

		virtual Reference<Image2D> get_image(uint32_t attachmentIndex) const = 0;
		virtual Reference<Image2D> get_depth_image() const = 0;

		virtual const FramebufferSpecification& get_specification() const = 0;

		static Reference<Framebuffer> create(const FramebufferSpecification& spec);
	};

} // namespace ForgottenEngine
