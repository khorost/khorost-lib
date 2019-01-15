#include "net/token.h"

int khorost::network::token::get_access_duration() const {
    return m_payload_.isMember("delta_access_time") ? m_payload_["delta_access_time"].asInt() : 0;
}

int khorost::network::token::get_refresh_duration() const {
    return m_payload_.isMember("delta_refresh_time") ? m_payload_["delta_refresh_time"].asInt() : 0;
}
