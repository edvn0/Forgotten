//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "VertexBuffer.hpp"

namespace ForgottenEngine {

class VulkanVertexBuffer : public VertexBuffer {
public:
	explicit VulkanVertexBuffer(size_t size)
		: size(size){};

	~VulkanVertexBuffer() override = default;

	std::string to_string() override;

private:
	size_t size;
};

} // ForgottenEngine