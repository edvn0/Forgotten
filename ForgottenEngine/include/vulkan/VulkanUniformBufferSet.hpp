//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/UniformBufferSet.hpp"

namespace ForgottenEngine {

class VulkanUniformBufferSet : public UniformBufferSet {
public:
	explicit VulkanUniformBufferSet(uint32_t size);
	void create(uint32_t size, uint32_t binding) override;

protected:
	Reference<UniformBuffer> get_impl(uint32_t binding, uint32_t set, uint32_t frame) override;
	void set_impl(Reference<UniformBuffer> buffer, uint32_t set, uint32_t frame) override;
};

}