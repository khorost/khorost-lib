#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <json/json.h>

namespace khorost {
    namespace data {
        std::string escape_string(const std::string& s_);
        boost::posix_time::time_duration epoch_diff(boost::posix_time::ptime pt_);
        boost::posix_time::ptime epoch_microseconds2ptime(uint64_t ms_);
        boost::posix_time::ptime epoch_milliseconds2ptime(uint64_t ms);

        std::string clear_html_tags(const std::string source);

        typedef boost::date_time::local_adjustor<boost::posix_time::ptime, +3, boost::posix_time::no_dst> tz_msk;
        typedef boost::date_time::local_adjustor<boost::posix_time::ptime, +2, boost::posix_time::no_dst> tz_cet;

        std::string json_string(const Json::Value& value);
        bool parse_json(char const* begin_doc, char const* end_doc, Json::Value& value);

        inline bool parse_json_string(const std::string& source, Json::Value& value) {
            return parse_json(source.c_str(), source.c_str() + source.size(), value);
        }

        inline void compact_uuid_to_string(std::string& uuid) {
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
        }
    }
}
