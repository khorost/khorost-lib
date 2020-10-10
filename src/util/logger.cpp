// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "util/logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>

void khorost::log::prepare_logger(const config& configure, const std::string& logger_name) {
    spdlog::init_thread_pool(8192, 1);
    spdlog::flush_every(std::chrono::seconds(10));

    const auto prefix_key = "log:" + logger_name;

    const auto pattern_file = configure.get_value(prefix_key + ":pattern", "[%Y-%m-%d %T.%F] [%n] [thread %t] [%l] %v");
    const auto pattern_console = configure.get_value(prefix_key + ":pattern",
                                                     "[%Y-%m-%d %T.%F] [%n] [thread %t] [%^%l%$] %v");
    const auto level = configure.get_value(prefix_key + ":level", "WARNING");
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        configure.get_value(prefix_key + ":file_name", "./log")
        , configure.get_value(prefix_key + ":max_file_size", 1048576 * 20)
        , configure.get_value(prefix_key + ":max_files", 10));
    rotating_sink->set_pattern(pattern_file);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern(pattern_console);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, rotating_sink};

    if (configure.is_value(prefix_key + ":async", true)) {
        static auto tp = std::make_shared<spdlog::details::thread_pool>(8192, 2); // TODO залепуха, исправить
        register_logger(std::make_shared<spdlog::async_logger>(logger_name, sinks.begin(), sinks.end(),
                                                                       tp,
                                                                       spdlog::async_overflow_policy::block));
    } else {
        register_logger(std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end()));
    }

    const auto logger = spdlog::get(logger_name);
    if (logger != nullptr) {
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

        logger->info("Logger prepare successful. level = {}", level);
    }
}
