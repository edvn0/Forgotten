//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

class Mesh {
public:
	virtual void upload() = 0;

public:
	static std::unique_ptr<Mesh> create(std::string path);
};

}