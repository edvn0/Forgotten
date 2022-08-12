#include "fg_pch.hpp"

#include "Logger.hpp"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <filesystem>

namespace ForgottenEngine {

	std::shared_ptr<spdlog::logger> Logger::core_logger;
	std::shared_ptr<spdlog::logger> Logger::client_logger;

	void Logger::init()
	{
		spdlog::set_pattern("%^[%T] %n: %v%$");
		core_logger = spdlog::stdout_color_mt("Engine");
		core_logger->set_level(spdlog::level::trace);
		client_logger = spdlog::stdout_color_mt("App");
		client_logger->set_level(spdlog::level::trace);
	}

	void Logger::shutdown()
	{
		client_logger.reset();
		core_logger.reset();
		spdlog::drop_all();
	}

} // namespace ForgottenEngine
