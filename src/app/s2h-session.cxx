#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <json/json.h>

#include "app/s2h-session.h"

using namespace khorost::Network;

S2HSession::S2HSession(const std::string& sSessionID_, boost::posix_time::ptime dtCreated, boost::posix_time::ptime dtExpired) :
    Session(sSessionID_, dtCreated, dtExpired) {
    Reset();
}

S2HSession::~S2HSession(){
}

bool S2HSession::ExportData(std::string& sData_) {
    Json::Value root, roles;
    Json::FastWriter writer;

    root["bAuthenticate"]   = m_bAuthenticate;
    root["sNickname"]       = m_sNickname;
    root["sPosition"] = m_sPosition;
    root["idUser"] = m_idUser;

    FillRoles(roles);

    root["Roles"] = roles;
    sData_ = writer.write(root);
    return Session::ExportData(sData_);
}

void S2HSession::FillRoles(Json::Value& jv_) {
    for (std::set<std::string>::const_iterator cit = m_dRoles.begin(); cit != m_dRoles.end(); ++cit) {
        jv_.append(*cit);
    }
}

void S2HSession::Reset() {
    m_bAuthenticate = false;
    m_idUser = 0;
    m_sNickname = "";
    m_sPosition = "участник";
    m_dRoles.empty();
}

bool S2HSession::ImportData(const std::string& sData_){
    Json::Value root, roles;
    Json::Reader reader;

    if (!reader.parse(sData_, root)) {
        return false;
    }

    m_bAuthenticate = root.get("bAuthenticate", false).asBool();
    m_sNickname     = root.get("sNickname", "").asString();
    m_sPosition = root.get("sPosition", "участник").asString();
    m_idUser = root.get("idUser", "").asInt();

    roles = root["Roles"];
    m_dRoles.empty();
    for (auto rit = roles.begin(); rit != roles.end(); ++rit) {
        m_dRoles.insert((*rit).asString());
    }

    return Session::ImportData(sData_);
}
