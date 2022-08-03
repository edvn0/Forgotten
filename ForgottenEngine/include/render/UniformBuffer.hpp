#pragma once

#include "Reference.hpp"

namespace ForgottenEngine {

class UniformBuffer : public ReferenceCounted {
public:
	virtual ~UniformBuffer() = default;
	void set_data(const void* data, uint32_t size, uint32_t offset = 0) { set_data_impl(data, size, offset); };
	void rt_set_data(const void* data, uint32_t size, uint32_t offset = 0)
	{
		rt_set_data_impl(data, size, offset);
	};

	virtual uint32_t get_binding() const = 0;

	static Reference<UniformBuffer> create(uint32_t size, uint32_t binding);

protected:
	virtual void rt_set_data_impl(const void* data, uint32_t size, uint32_t offset) = 0;
	virtual void set_data_impl(const void* data, uint32_t size, uint32_t offset) = 0;
};

}
