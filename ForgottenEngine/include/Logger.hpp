#pragma once

// clang-format off
#define YAML_CPP_STATIC_DEFINE

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//clang-format on


namespace ForgottenEngine {

class Logger {
public:
	static void init();
	static void shutdown();

	static std::shared_ptr<spdlog::logger> get_core_logger() { return core_logger; };
	static std::shared_ptr<spdlog::logger> get_client_logger() { return client_logger; };

private:
	static std::shared_ptr<spdlog::logger> core_logger;
	static std::shared_ptr<spdlog::logger> client_logger;
};

}

#define CORE_WARN(...) ::ForgottenEngine::Logger::get_core_logger()->warn(__VA_ARGS__)
#define CORE_INFO(...) ::ForgottenEngine::Logger::get_core_logger()->info(__VA_ARGS__)
#define CORE_DEBUG(...) ::ForgottenEngine::Logger::get_core_logger()->debug(__VA_ARGS__)
#define CORE_TRACE(...) ::ForgottenEngine::Logger::get_core_logger()->trace(__VA_ARGS__)
#define CORE_ERROR(...) ::ForgottenEngine::Logger::get_core_logger()->error(__VA_ARGS__)

#define CLIENT_ERROR(...) ::ForgottenEngine::Logger::get_client_logger()->error(__VA_ARGS__)
#define CLIENT_WARN(...) ::ForgottenEngine::Logger::get_client_logger()->warn(__VA_ARGS__)
#define CLIENT_INFO(...) ::ForgottenEngine::Logger::get_client_logger()->info(__VA_ARGS__)
#define CLIENT_DEBUG(...) ::ForgottenEngine::Logger::get_client_logger()->debug(__VA_ARGS__)
#define CLIENT_TRACE(...) ::ForgottenEngine::Logger::get_client_logger()->trace(__VA_ARGS__)
