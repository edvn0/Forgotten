#pragma once

// clang-format off
#define YAML_CPP_STATIC_DEFINE

#include "Common.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//clang-format on


namespace ForgottenEngine {

class Logger {
public:
	static void init();

	static std::shared_ptr<spdlog::logger> get_core_logger();
	static std::shared_ptr<spdlog::logger> get_client_logger();

private:
	static std::shared_ptr<spdlog::logger> core_logger;
	static std::shared_ptr<spdlog::logger> client_logger;
};

}

#define CORE_ERROR(...)                                                                                       \
    do {                                                                                                      \
        ::ForgottenEngine::Logger::get_core_logger()->error(__VA_ARGS__);                                     \
        debug_break();                                                                                              \
	} while (0)

#define CORE_WARN(...) ::ForgottenEngine::Logger::get_core_logger()->warn(__VA_ARGS__)
#define CORE_INFO(...) ::ForgottenEngine::Logger::get_core_logger()->info(__VA_ARGS__)
#define CORE_DEBUG(...) ::ForgottenEngine::Logger::get_core_logger()->debug(__VA_ARGS__)
#define CORE_TRACE(...) ::ForgottenEngine::Logger::get_core_logger()->trace(__VA_ARGS__)
#define CORE_ERR(...) ::ForgottenEngine::Logger::get_core_logger()->error(__VA_ARGS__)

#define CLIENT_ERROR(...) ::ForgottenEngine::Logger::get_client_logger()->error(__VA_ARGS__)
#define CLIENT_WARN(...) ::ForgottenEngine::Logger::get_client_logger()->warn(__VA_ARGS__)
#define CLIENT_INFO(...) ::ForgottenEngine::Logger::get_client_logger()->info(__VA_ARGS__)
#define CLIENT_DEBUG(...) ::ForgottenEngine::Logger::get_client_logger()->debug(__VA_ARGS__)
#define CLIENT_TRACE(...) ::ForgottenEngine::Logger::get_client_logger()->trace(__VA_ARGS__)


#define DEBUG_BREAK debug_break();
#define ENABLE_ASSERTS
#define ENABLE_VERIFY

#ifdef ENABLE_ASSERTS
	#define CORE_ASSERT_MESSAGE_INTERNAL(...)  ::ForgottenEngine::Logger::get_core_logger()->error("Assertion failed", __VA_ARGS__)
	#define ASSERT_MESSAGE_INTERNAL(...)  ::ForgottenEngine::Logger::get_client_logger()->error("Assertion failed", __VA_ARGS__)

	#define CORE_ASSERT(condition, ...) { if(!(condition)) { CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DEBUG_BREAK; } }
	#define ASSERT(condition, ...) { if(!(condition)) { ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DEBUG_BREAK; } }
#else
	#define CORE_ASSERT(condition, ...)
	#define ASSERT(condition, ...)
#endif

#ifdef ENABLE_VERIFY
	#define CORE_VERIFY_MESSAGE_INTERNAL(...)  ::ForgottenEngine::Logger::get_core_logger()->warn("Verify failed", __VA_ARGS__)
	#define VERIFY_MESSAGE_INTERNAL(...)  ::ForgottenEngine::Logger::get_core_logger()->warn("Verify failed", __VA_ARGS__)

	#define CORE_VERIFY(condition, ...) { if(!(condition)) { CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DEBUG_BREAK; } }
	#define VERIFY(condition, ...) { if(!(condition)) { VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DEBUG_BREAK; } }
#else
	#define CORE_VERIFY(condition, ...)
	#define VERIFY(condition, ...)
#endif

