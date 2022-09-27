#pragma once

#include "events/Event.hpp"

#include <string>
#include <string_view>

namespace ForgottenEngine {

	class TimeStep;

	class Layer {
	private:
		std::string name;
		bool should_block;

	public:
		explicit Layer(std::string name)
			: name(std::move(name)) {};

		virtual ~Layer() = default;

		virtual void on_attach() {};
		virtual void on_event(Event& e) {};
		virtual void on_update(const TimeStep& ts) {};
		virtual void on_ui_render(const TimeStep& ts) {};
		virtual void on_detach() {};

		virtual void block(bool in_should_block) { should_block = in_should_block; };

		virtual std::string_view get_name() { return name; }
	};

} // namespace ForgottenEngine
