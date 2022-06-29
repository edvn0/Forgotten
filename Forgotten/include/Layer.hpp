#pragma once

namespace Forgotten {

class Layer {
public:
	virtual ~Layer() = default;
	virtual void on_attach(){};
	virtual void on_detach(){};
	virtual void on_render(){};
	virtual void on_ui_render(){};
};

} // namespace Forgotten
