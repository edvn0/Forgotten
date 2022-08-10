//
// Created by Edwin Carlsson on 2022-08-03.
//

#pragma once

#include "render/StorageBufferSet.hpp"

namespace ForgottenEngine {

	class VulkanStorageBufferSet : public StorageBufferSet {
	public:
		explicit VulkanStorageBufferSet(uint32_t size);

		void create(uint32_t size, uint32_t binding) override;
		void resize(uint32_t binding, uint32_t set, uint32_t newSize) override;

	protected:
		Reference<StorageBuffer> get_impl(uint32_t binding, uint32_t set, uint32_t frame) override;
		void set_impl(Reference<StorageBuffer> storageBuffer, uint32_t set, uint32_t frame) override;
	};

} // namespace ForgottenEngine