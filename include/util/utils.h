#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace khorost {
    namespace data {
        std::string escape_string(const std::string& s_);
        boost::posix_time::time_duration epoch_diff(boost::posix_time::ptime pt_);
        boost::posix_time::ptime epoch_microseconds2ptime(uint64_t ms_);
        boost::posix_time::ptime epoch_milliseconds2ptime(uint64_t ms);

        std::string clear_html_tags(const std::string source);
    }
}