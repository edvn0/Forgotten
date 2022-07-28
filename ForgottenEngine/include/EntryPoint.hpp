#pragma once

#include "ApplicationProperties.hpp"

extern ForgottenEngine::Application* ForgottenEngine::create_application(const ApplicationProperties& props);

#include <argparse/argparse.hpp>
#include <memory>
#include <system_error>

#include <filesystem>

int main(int argc, char** argv)
{
	ForgottenEngine::Application* app{ nullptr };
	ForgottenEngine::Logger::init();

	auto cwd = std::filesystem::current_path();
	CORE_INFO("{}", cwd);

	argparse::ArgumentParser program("main");

	program.add_argument("--width");
	program.add_argument("--height");
	program.add_argument("--name");
	// program.add_argument("--vsync").default_value(true);

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		CORE_ERR("{}", err.what());
		CORE_ERR("{}", program);
		std::exit(1);
	}

	ForgottenEngine::ApplicationProperties props;
	props.title = program.get("--name");
	props.width = std::stoi(program.get("--width"));
	props.height = std::stoi(program.get("--height"));
	props.v_sync = true;

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

	delete app;
}
