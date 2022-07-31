#pragma once

#include "UniformBuffer.hpp"

namespace ForgottenEngine {

    class UniformBufferSet : public ReferenceCounted
    {
    public:
        virtual ~UniformBufferSet() = default;

        virtual void create(uint32_t size, uint32_t binding) = 0;

        Reference<UniformBuffer> get(uint32_t binding, uint32_t set = 0, uint32_t frame = 0) { return get_impl(binding, set, frame); }
        void set(const Reference<UniformBuffer>& buffer, uint32_t set = 0, uint32_t frame = 0) { set_impl(buffer, set, frame); }

        static Reference<UniformBufferSet> create(uint32_t frames);

    protected:
        virtual Reference<UniformBuffer> get_impl(uint32_t binding, uint32_t set, uint32_t frame) = 0;
        virtual void set_impl(Reference<UniformBuffer> buffer, uint32_t set, uint32_t frame) = 0;

    };

}
