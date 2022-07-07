//
// Created by Edwin Carlsson on 2022-07-02.
//

#include "vulkan/VulkanEngine.hpp"

int main(int argc, char** argv)
{
	ForgottenEngine::VulkanEngine engine({ .w = 1500, .h = 850, .name = "Forgotten" });
	if (engine.initialize()) {
		engine.run();
	}

	engine.cleanup();

	return 0;
}