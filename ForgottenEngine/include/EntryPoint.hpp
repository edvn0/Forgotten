#pragma once

#include "ApplicationProperties.hpp"

extern ForgottenEngine::Application* ForgottenEngine::create_application(const ApplicationProperties& props);

#include <argparse/argparse.hpp>
#include <memory>
#include <system_error>

int main(int argc, char** argv)
{
	ForgottenEngine::Application* app{ nullptr };
	ForgottenEngine::Logger::init();

	argparse::ArgumentParser program("main");

	program.add_argument("--width");
	program.add_argument("--height");
	program.add_argument("--name");

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		std::exit(1);
	}

	ForgottenEngine::ApplicationProperties props;
	props.title = program.get("--name");
	props.width = std::stoi(program.get("--width"));
	props.height = std::stoi(program.get("--height"));

	try {
		app = ForgottenEngine::create_application(props);
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
