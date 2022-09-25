#pragma once

#include "Reference.hpp"

namespace ForgottenEngine {

	class UniformBuffer : public ReferenceCounted {
	public:
		virtual ~UniformBuffer() = default;
		virtual void set_data(const void* data, uint32_t size, uint32_t offset) = 0;
		virtual void render_thread_set_data(const void* data, uint32_t size, uint32_t offset) = 0;

		virtual uint32_t get_binding() const = 0;

		static Reference<UniformBuffer> create(uint32_t size, uint32_t binding);
	};

} // namespace ForgottenEngine
