#include "util/logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

void khorost::log::prepare_logger(const config& configure, const std::string& logger_name) {
    spdlog::flush_every(std::chrono::seconds(5));

    const auto pattern = configure.get_value("log:pattern", "[%Y-%m-%d %T.%F] [%n] [thread %t] [%^%l%$] %v");
    const auto level = configure.get_value("log:level", "WARNING");
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(configure.get_value("log:file_name", "./log")
                                                                                , configure.get_value("log:max_file_size", 1048576 * 5)
                                                                                , configure.get_value("log:max_files", 3));
    rotating_sink->set_pattern(pattern);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern(pattern);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, rotating_sink};
    auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());

    if (level == "DEBUG") {
        logger->set_level(spdlog::level::debug);
    } else if (level == "TRACE") {
        logger->set_level(spdlog::level::trace);
    } else if (level == "INFO") {
        logger->set_level(spdlog::level::info);
    } else if (level == "ERROR") {
        logger->set_level(spdlog::level::err);
    } else if (level == "CRITICAL") {
        logger->set_level(spdlog::level::critical);
    } else if (level == "OFF") {
        logger->set_level(spdlog::level::off);
    } else {
        logger->set_level(spdlog::level::warn);
    }

    spdlog::register_logger(logger);

    logger->info("logger prepare successful. level = {}", level);
}
