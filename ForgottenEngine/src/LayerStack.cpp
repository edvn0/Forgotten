#include "fg_pch.hpp"

#include "LayerStack.hpp"

#include "imgui/ImGuiLayer.hpp"

namespace ForgottenEngine {

	LayerStack::LayerStack() { layer_insert = 0; }

	LayerStack::~LayerStack() = default;

	void LayerStack::push(std::unique_ptr<Layer> layer)
	{
		layer->on_attach();
		layer_stack.emplace(begin() + layer_insert, std::move(layer));
		layer_insert++;
	}

	void LayerStack::push_overlay(std::unique_ptr<Layer> overlay)
	{
		overlay->on_attach();
		layer_stack.emplace_back(std::move(overlay));
	}

	void LayerStack::pop(std::unique_ptr<Layer> layer)
	{
		auto found = std::find(begin(), end(), layer);
		if (found != end()) {
			layer_stack.erase(found);
			layer_insert--;
		}
	}

	void LayerStack::pop_overlay(std::unique_ptr<Layer> overlay)
	{
		auto found = std::find(begin(), end(), overlay);
		if (found != end()) {
			layer_stack.erase(found);
		}
	}

} // namespace ForgottenEngine
