//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include "Common.hpp"
#include <string>

namespace ForgottenEngine {

class VertexBuffer {
public:
	virtual ~VertexBuffer() = default;

	virtual std::string to_string() = 0;

public:
	static std::shared_ptr<VertexBuffer> create(size_t size);
};

}