#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace khorost {
    namespace data {
        std::string EscapeString(const std::string& s_);
        boost::posix_time::time_duration    EpochDiff(boost::posix_time::ptime pt_);
        boost::posix_time::ptime            EpochMicroseconds2ptime(uint64_t ms_);

        std::string clear_html_tags(const std::string source);
    }
}