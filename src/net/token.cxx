// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "net/token.h"

int khorost::network::token::get_access_duration() const {
    return m_payload_.isMember(khl_json_param_delta_access_time) ? m_payload_[khl_json_param_delta_access_time].asInt() : 0;
}

int khorost::network::token::get_refresh_duration() const {
    return m_payload_.isMember(khl_json_param_delta_refresh_time) ? m_payload_[khl_json_param_delta_refresh_time].asInt() : 0;
}
