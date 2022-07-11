#pragma once

extern ForgottenEngine::Application* ForgottenEngine::create_application();

#include <memory>
#include <system_error>

int main(int argc, char** argv)
{
	ForgottenEngine::Application* app{ nullptr };
	ForgottenEngine::Logger::init();

	try {
		app = ForgottenEngine::create_application();
	} catch (const std::system_error& e) {
		CORE_INFO("{}", e.what());
	}

	try {
		app->run();
	} catch (const std::system_error& e) {
		CORE_INFO("{}", e.what());
	}

	try {
		app->cleanup();
		delete app;
	} catch (const std::system_error& e) {
		CORE_INFO("{}", e.what());
	}
}
