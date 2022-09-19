#pragma once

#include "ApplicationProperties.hpp"

extern ForgottenEngine::Application* ForgottenEngine::create_application(const ApplicationProperties& props);

#include "yaml-cpp/yaml.h"

#include <any>
#include <boost/program_options.hpp>
#include <filesystem>
#include <memory>
#include <system_error>

namespace ForgottenEngine {
	using CLIOptions = boost::program_options::options_description;
	using ArgumentMap = boost::program_options::variables_map;
} // namespace ForgottenEngine

int main(int argc, char** argv)
{
	ForgottenEngine::Application* app { nullptr };
	ForgottenEngine::Assets::init();
	ForgottenEngine::Logger::init();

	auto cwd = std::filesystem::current_path();
	CORE_INFO("{}", cwd);

	std::filesystem::path defaults_path = cwd / std::filesystem::path { "resources" } / std::filesystem::path { "cli_defaults.yml" };

	YAML::Node config;
	try {
		config = YAML::LoadFile(defaults_path.string());
	} catch (const YAML::BadFile& bad) {
		CORE_ERROR("Could not load CLI Defaults.");
		std::exit(1);
	}

	if (config["width"]) {
		CORE_TRACE("Found width with value: {}", config["width"].as<uint32_t>());
	}

	ForgottenEngine::CLIOptions desc("Allowed options");
	desc.add_options()("help", "Show help message")("width", boost::program_options::value<uint32_t>()->default_value(1280), "Width of window")(
		"height", boost::program_options::value<uint32_t>()->default_value(720), "Height of window")("name",
		boost::program_options::value<std::string>()->default_value(std::string { "ForgottenEngine" }),
		"Title of window")("vsync", boost::program_options::value<bool>()->default_value(true), "Window vsync");

	ForgottenEngine::ArgumentMap vm;
	try {
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);
	} catch (const std::runtime_error& err) {
		CORE_ERROR("EntryPoint Error: {}", err.what());
		std::exit(1);
	}

	ForgottenEngine::ApplicationProperties props;

	props.title = [vm]() {
		try {
			return vm["name"].as<std::string>();
		} catch (const std::bad_any_cast& bad_any_cast) {
			CORE_ERROR("Bad cast, Name: {}", bad_any_cast.what());
			std::exit(1);
		}
	}();

	props.width = [vm]() {
		try {
			return vm["width"].as<uint32_t>();
		} catch (const std::bad_any_cast& bad_any_cast) {
			CORE_ERROR("Bad cast, Width: {}", bad_any_cast.what());
			std::exit(1);
		}
	}();

	props.height = [vm]() {
		try {
			return vm["height"].as<uint32_t>();
		} catch (const std::bad_any_cast& bad_any_cast) {
			CORE_ERROR("Bad cast, Height: {}", bad_any_cast.what());
			std::exit(1);
		}
	}();

	props.v_sync = [vm]() {
		try {
			return vm["vsync"].as<bool>();
		} catch (const std::bad_any_cast& bad_any_cast) {
			CORE_ERROR("Bad cast, Height: {}", bad_any_cast.what());
			std::exit(1);
		}
	}();

	CORE_TRACE("{}, {}, {}, {}", props.width, props.height, props.title, props.v_sync);

	try {
		app = ForgottenEngine::create_application(props);
	} catch (const std::system_error& e) {
		CORE_INFO("Error in app creation: {}", e.what());
	}

	try {
		app->run();
	} catch (const std::system_error& e) {
		CORE_INFO("{}", e.what());
	}

	Logger::shutdown();

	delete app;
}
