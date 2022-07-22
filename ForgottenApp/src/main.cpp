#include "ForgottenLayer.hpp"

#include "Application.hpp"
#include "EntryPoint.hpp"

#include "render/Renderer.hpp"

using namespace ForgottenEngine;

class ForgottenApp : public Application {
public:
	ForgottenApp(const ApplicationProperties& props)
		: Application(props)
	{
		Renderer::init();
		auto* window_handle = static_cast<GLFWwindow*>(window->get_natively());
		auto engine = std::make_unique<VulkanEngine>(props);
		ForgottenEngine::VulkanContext::construct_and_initialize(window_handle);
		engine->initialize();
		set_engine(std::move(engine));
	};

	~ForgottenApp() override = default;

	void on_init() override { add_layer(std::make_unique<ForgottenLayer>()); }
};

ForgottenEngine::Application* ForgottenEngine::create_application(const ApplicationProperties& props)
{
	return new ForgottenApp(props);
}
