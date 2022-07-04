#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-loop-convert"
//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include <deque>

namespace ForgottenEngine {

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) { deletors.push_back(function); }

	void flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			const auto& deletion_function = *it;
			deletion_function(); // call the function
		}

		deletors.clear();
	}
};

} // ForgottenEngine
#pragma clang diagnostic pop