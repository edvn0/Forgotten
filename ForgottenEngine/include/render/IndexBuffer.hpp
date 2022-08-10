#pragma once

#include "Reference.hpp"

namespace ForgottenEngine {

	class IndexBuffer : public ReferenceCounted {
	public:
		virtual ~IndexBuffer() { }

		virtual void set_data(void* buffer, uint32_t size, uint32_t offset = 0) = 0;
		virtual void bind() const = 0;

		virtual uint32_t get_count() const = 0;

		virtual uint32_t get_size() const = 0;
		virtual RendererID get_renderer_id() const = 0;

		static Reference<IndexBuffer> create(uint32_t size);
		static Reference<IndexBuffer> create(void* data, uint32_t size = 0);
	};

} // namespace ForgottenEngine
