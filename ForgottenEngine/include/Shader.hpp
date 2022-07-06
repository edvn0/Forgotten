#pragma once

#include <string>

struct VkShaderModule_T;
typedef VkShaderModule_T* VkShaderModule;

namespace ForgottenEngine {

class Shader {
protected:
	virtual bool load_shader_from_file(std::string path) = 0;

public:
	virtual ~Shader() = default;
	virtual void destroy() = 0;
	virtual VkShaderModule get_module() = 0;
	static std::unique_ptr<Shader> create(std::string path);
};

}