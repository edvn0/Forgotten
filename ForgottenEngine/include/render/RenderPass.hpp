#pragma once

#include "Common.hpp"

#include "Framebuffer.hpp"

namespace ForgottenEngine {

    struct RenderPassSpecification
    {
        Reference<Framebuffer> TargetFramebuffer;
        std::string DebugName;
        glm::vec4 MarkerColor;
    };

    class RenderPass : public ReferenceCounted
    {
    public:
        virtual ~RenderPass() = default;

        virtual RenderPassSpecification& GetSpecification() = 0;
        virtual const RenderPassSpecification& GetSpecification() const = 0;

        static Reference<RenderPass> create(const RenderPassSpecification& spec);
    };

}