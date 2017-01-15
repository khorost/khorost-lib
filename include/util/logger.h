#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <g3log/std2_make_unique.hpp>

namespace khorost {
    namespace log {
        void appendColorSink(g3::LogWorker* logger_);
    }
}

#endif	// __LOGGER_H__
