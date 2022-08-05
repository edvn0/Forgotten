#pragma once

#include "UniformBuffer.hpp"

namespace ForgottenEngine {

class UniformBufferSet : public ReferenceCounted {
public:
	virtual ~UniformBufferSet() = default;

	virtual void create(uint32_t size, uint32_t binding) = 0;

	virtual Reference<UniformBuffer> get(uint32_t binding, uint32_t set, uint32_t frame) = 0;
	virtual void set(const Reference<UniformBuffer>& buffer, uint32_t set, uint32_t frame) = 0;

	static Reference<UniformBufferSet> create(uint32_t frames);
};

}
