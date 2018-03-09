#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <json/json.h>

#include "app/s2h-session.h"

using namespace khorost::Network;

S2HSession::S2HSession(const std::string& sSessionID_, boost::posix_time::ptime dtCreated,
                       boost::posix_time::ptime dtExpired) :
    Session(sSessionID_, dtCreated, dtExpired) {
    reset();
}

bool S2HSession::ExportData(std::string& data) {
    Json::Value root, roles;
    Json::FastWriter writer;

    root["bAuthenticate"] = m_bAuthenticate;
    root["sNickname"] = m_sNickname;
    root["sPosition"] = m_sPosition;
    root["idUser"] = m_idUser;

    fill_roles(roles);

    root["Roles"] = roles;
    data = writer.write(root);

    return Session::ExportData(data);
}

void S2HSession::fill_roles(Json::Value& value) {
    for (const auto& r : m_dRoles) {
        value.append(r);
    }
}

void S2HSession::reset() {
    m_bAuthenticate = false;
    m_idUser = 0;
    m_sNickname = "";
    m_sPosition = "участник";
    m_dRoles.clear();
}

bool S2HSession::ImportData(const std::string& data) {
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(data, root)) {
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

    return Session::ImportData(data);
}
