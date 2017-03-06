#include "util/text.h"

std::string EscapeString(const std::string& s_) {
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
