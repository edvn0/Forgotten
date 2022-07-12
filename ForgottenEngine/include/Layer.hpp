#pragma once

#include <string>
#include <string_view>

#include "events/Event.hpp"

namespace ForgottenEngine {

class TimeStep;

class Layer {
private:
	std::string name;

public:
	explicit Layer(std::string name)
		: name(std::move(name)){};

	virtual ~Layer() = default;

	virtual void on_attach(){};
	virtual void on_event(Event& e){};
	virtual void on_update(const TimeStep& ts){};
	virtual void on_ui_render(const TimeStep& ts){};
	virtual void on_detach(){};

	virtual std::string_view get_name() { return name; }
};

} // namespace ForgottenEngine
