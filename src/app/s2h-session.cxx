// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <json/json.h>
#include <util/utils.h>

#include "app/s2h-session.h"

using namespace khorost::network;

void s2h_session::get_expire_shift(boost::posix_time::time_duration& value) {
    if (m_bAuthenticate) {
        value = boost::posix_time::hours(24 * 14);
    } else {
        session::get_expire_shift(value);
    }
}

bool s2h_session::export_data(std::string& data) {
    Json::Value root, roles;

    root["bAuthenticate"] = m_bAuthenticate;
    root["sNickname"] = m_sNickname;
    root["sPosition"] = m_sPosition;
    root["idUser"] = m_idUser;

    fill_roles(roles);

    root["Roles"] = roles;
    data = data::json_string(root);

    return session::export_data(data);
}

void s2h_session::fill_roles(Json::Value& value) {
    for (const auto& r : m_dRoles) {
        value.append(r);
    }
}

void s2h_session::reset() {
    session::reset();

    m_bAuthenticate = false;
    m_idUser = 0;
    m_sNickname = "";
    m_sPosition = "участник";
    m_dRoles.clear();
}

bool s2h_session::import_data(const std::string& data) {
    Json::Value root;

    if (!data::parse_json_string(data, root)) {
        return false;
    }

    reset();

    m_bAuthenticate = root.get("bAuthenticate", false).asBool();
    m_sNickname = root.get("sNickname", "").asString();
    m_sPosition = root.get("sPosition", "участник").asString();
    m_idUser = root.get("idUser", "").asInt();

    const auto roles = root["Roles"];

    for (const auto& r : roles) {
        m_dRoles.insert(r.asString());
    }

    return session::import_data(data);
}
