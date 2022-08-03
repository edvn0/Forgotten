#pragma once

#include "StorageBuffer.hpp"

namespace ForgottenEngine {

class StorageBufferSet : public ReferenceCounted {
public:
	virtual ~StorageBufferSet() = default;

	virtual void create(uint32_t size, uint32_t binding) = 0;

	Reference<StorageBuffer> get(uint32_t binding, uint32_t set = 0, uint32_t frame = 0)
	{
		return get_impl(binding, set, frame);
	};
	void set(Reference<StorageBuffer> storageBuffer, uint32_t set = 0, uint32_t frame = 0)
	{
		return set_impl(storageBuffer, set, frame);
	};
	virtual void resize(uint32_t binding, uint32_t set, uint32_t newSize) = 0;

	static Reference<StorageBufferSet> create(uint32_t frames);

protected:
	virtual Reference<StorageBuffer> get_impl(uint32_t binding, uint32_t set, uint32_t frame) = 0;
	virtual void set_impl(Reference<StorageBuffer> storageBuffer, uint32_t set, uint32_t frame) = 0;
};

}
