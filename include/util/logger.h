#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>

#define G3_DYNAMIC_LOGGING
#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <g3log/std2_make_unique.hpp>

namespace khorost {
    namespace log{
        void prepare(const std::string& sFolder_ = "", const std::string& sPrefix_ = "", const std::string& sID_ = "s2");
    }
}

#endif	// __LOGGER_H__
