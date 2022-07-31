#pragma once

#include "Common.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ForgottenEngine {

enum class ShaderDomain {
	None = 0,
	Vertex = 0,
	Pixel = 1 // unused
};

class ShaderResourceDeclaration {
public:
	ShaderResourceDeclaration() = default;
	ShaderResourceDeclaration(std::string  name, uint32_t resource_register, uint32_t count)
		: name(std::move(name))
		, resource_register(resource_register)
		, count(count)
	{
	}

	virtual const std::string& get_name() const { return name; }
	virtual uint32_t get_register() const { return resource_register; }
	virtual uint32_t get_count() const { return count; }

private:
	std::string name;
	uint32_t resource_register = 0;
	uint32_t count = 0;
};

typedef std::vector<ShaderResourceDeclaration*> ShaderResourceList;

}
