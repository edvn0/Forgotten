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
			: Format(format)
		{
		}

		ImageFormat Format { ImageFormat::None };
		bool Blend = true;
		FramebufferBlendMode BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
		// TODO: filtering/wrap
	};

	struct FramebufferAttachmentSpecification {
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(const std::initializer_list<FramebufferTextureSpecification>& attachments)
			: texture_attachments(attachments)
		{
		}

		std::vector<FramebufferTextureSpecification> texture_attachments;
	};

	struct FramebufferSpecification {
		float Scale = 1.0f;
		uint32_t Width = 0;
		uint32_t Height = 0;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		float DepthClearValue = 0.0f;
		bool ClearColorOnLoad = true;
		bool ClearDepthOnLoad = true;

		FramebufferAttachmentSpecification attachments;
		uint32_t Samples = 1; // multisampling

		// TODO: Temp, needs scale
		bool NoResize = false;

		// Master switch (individual attachments can be disabled in FramebufferTextureSpecification)
		bool Blend = true;
		// None means use BlendMode in FramebufferTextureSpecification
		FramebufferBlendMode BlendMode = FramebufferBlendMode::None;

		// SwapChainTarget = screen buffer (i.e. no framebuffer)
		bool SwapChainTarget = false;

		// Note: these are used to attach multi-layered color/depth images
		Reference<Image2D> ExistingImage;
		std::vector<uint32_t> ExistingImageLayers;

		// Specify existing images to attach instead of creating
		// new images. attachment index -> image
		std::map<uint32_t, Reference<Image2D>> ExistingImages;

		// At the moment this will just create a new render pass
		// with an existing framebuffer
		Reference<Framebuffer> ExistingFramebuffer;

		std::string DebugName;
		bool Transfer;
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
