#include <string>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace khorost {
    namespace Data {
        std::string EscapeString(const std::string& s_);
        boost::posix_time::time_duration    EpochDiff(boost::posix_time::ptime pt_);
    }
}