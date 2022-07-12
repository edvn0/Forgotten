#include "ForgottenLayer.hpp"

#include "Application.hpp"
#include "EntryPoint.hpp"

using namespace ForgottenEngine;

class ForgottenApp : public Application {
public:
	ForgottenApp()
		: Application(WindowProps("Forgotten", 1280, 720))
	{
		add_layer(std::make_unique<ForgottenLayer>());
		auto* window_handle = static_cast<GLFWwindow*>(window->get_natively());
		auto engine = std::make_unique<VulkanEngine>(WindowProps("Forgotten", 1280, 720));
		ForgottenEngine::VulkanContext::construct_and_initialize(window_handle);
		engine->initialize();
		set_engine(std::move(engine));
	};

	~ForgottenApp() override = default;

	void cleanup() override { engine->cleanup(); }
};

ForgottenEngine::Application* ForgottenEngine::create_application() { return new ForgottenApp(); }
