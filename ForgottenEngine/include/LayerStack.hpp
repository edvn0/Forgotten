#pragma once

#include "Common.hpp"
#include "Layer.hpp"

namespace ForgottenEngine {

	class LayerStack {
		typedef std::vector<std::unique_ptr<Layer>> stack;
		typedef std::vector<std::unique_ptr<Layer>>::iterator stack_it;
		typedef std::vector<std::unique_ptr<Layer>>::reverse_iterator stack_it_rev;

	public:
		LayerStack();
		~LayerStack();

		void push(std::unique_ptr<Layer> layer);
		void push_overlay(std::unique_ptr<Layer> overlay);
		void pop(std::unique_ptr<Layer> layer);
		void pop_overlay(std::unique_ptr<Layer> overlay);

		stack_it begin() { return layer_stack.begin(); }
		stack_it end() { return layer_stack.end(); }
		stack_it_rev rbegin() { return layer_stack.rbegin(); };
		stack_it_rev rend() { return layer_stack.rend(); };

		Layer* get_imgui_layer()
		{
			// Technically we as programmers are the only ones who can change if imgui is not the first...
			// Must be changed if we add overlays...
			return layer_stack.back().get();
		}

	private:
		stack layer_stack = {};
		unsigned int layer_insert;
	};

} // namespace ForgottenEngine
