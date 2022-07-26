#include "ForgottenLayer.hpp"

#include "Application.hpp"
#include "EntryPoint.hpp"

#include "render/Renderer.hpp"

using namespace ForgottenEngine;

class ForgottenApp : public Application {
public:
	explicit ForgottenApp(const ApplicationProperties& props)
		: Application(props){};

	~ForgottenApp() override = default;

	void on_init() override { add_layer(std::make_unique<ForgottenLayer>()); }
};

ForgottenEngine::Application* ForgottenEngine::create_application(const ApplicationProperties& props)
{
	return new ForgottenApp(props);
}
