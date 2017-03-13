#include "util/utils.h"

std::string khorost::Data::EscapeString(const std::string& s_) {
    std::string r;

    for (std::string::const_iterator cit=s_.begin();cit!=s_.end();++cit) {
        if (*cit=='\'') {
            r.append("\\\'");
        } else if (*cit=='\n') {
            r.append("\\n");
        } else if (*cit=='\r') {
        } else {
            r.push_back(*cit);
        }
    }
    return r;
}

boost::posix_time::time_duration khorost::Data::EpochDiff(boost::posix_time::ptime pt_) {
    static boost::posix_time::ptime               time_t_epoch(boost::gregorian::date(1970, 1, 1));
    return pt_ - time_t_epoch;
}
