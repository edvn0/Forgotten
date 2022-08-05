//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/UniformBufferSet.hpp"

namespace ForgottenEngine {

class VulkanUniformBufferSet : public UniformBufferSet {
public:
	explicit VulkanUniformBufferSet(uint32_t size);
	~VulkanUniformBufferSet() = default;
	void create(uint32_t size, uint32_t binding) override;

	Reference<UniformBuffer> get(uint32_t binding, uint32_t set, uint32_t frame) override;
	void set(const Reference<UniformBuffer>& buffer, uint32_t set, uint32_t frame) override;

private:
	uint32_t frames;
	std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, Reference<UniformBuffer>>>>
		frame_ubs; // frame->set->binding
};

}