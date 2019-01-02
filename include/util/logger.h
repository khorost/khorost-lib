#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "app/config.h"

namespace khorost {
    namespace log {
        void prepare_logger(const config& configure, const std::string& logger_name);
    }
}

#endif	// __LOGGER_H__
