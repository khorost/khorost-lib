#include "app/s2hb-storage.h"
#include "util/logger.h"

#if defined(_WIN32) || defined(_WIN64)
 #include <Ws2tcpip.h>
#else
 #include <arpa/inet.h>
 #include <netdb.h>
#endif

#include <vector>
#include <string>
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <openssl/md5.h>

using namespace khorost::Network;
using namespace khorost::DB;

static void session_ip_update(S2HSession* httpSession_, pqxx::work& txn_) {
    std::list<std::string>  lIPs;
    httpSession_->GetIP(lIPs, true);
    for (auto it : lIPs) {
        txn_.exec(
            "INSERT INTO admin.khl_sessions_ip (id, ip) "
            " VALUES('" + httpSession_->GetSessionID() + "', '" + it + "' )"
            "  ON CONFLICT (id, ip) DO NOTHING "
            );
    }
}

void S2HBStorage::SessionIPUpdate() {
    pqxx::work txn(*m_rDB.m_dbConnection);

    pqxx::result r = txn.exec(
        "SELECT DISTINCT ip "
        " FROM admin.khl_sessions_ip "
        " WHERE host is NULL " 
        " LIMIT 10"
        );

    if (r.size() != 0) {
        for (auto row : r) {
            struct sockaddr_in saGNI = { 0 };
            char    hostname[1024] = "NULL", servInfo[1024];

            saGNI.sin_family = AF_INET;

            auto sIP = row[0].as<std::string>();

            LOG(DEBUG) << "[IP_UPDATE] check('" << sIP << "')";

            inet_pton(AF_INET, sIP.c_str(), &saGNI.sin_addr);
            if (getnameinfo((struct sockaddr *)&saGNI, sizeof(struct sockaddr), hostname, sizeof(hostname), servInfo, sizeof(servInfo), NI_NUMERICSERV) == 0) {
                LOG(DEBUG) << "[IP_UPDATE] getnameinfo('" << sIP << "') = '" << hostname << "'"  ;
                txn.exec(
                    "UPDATE admin.khl_sessions_ip "
                    " SET host = '" + std::string(hostname) + "' "
                    " WHERE ip = '" + sIP + "' AND host is NULL ");
            } else {
                LOG(DEBUG) << "[IP_UPDATE] getnameinfo('" << sIP << "') failed";
                txn.exec(
                    "UPDATE admin.khl_sessions_ip "
                    " SET host = 'n/a' "
                    " WHERE ip = '" + sIP + "' AND host is NULL ");
            }
        }
        txn.commit();
    }
}

bool S2HBStorage::SessionUpdate(khorost::Network::ListSession& rLS_) {
    pqxx::work txn(*m_rDB.m_dbConnection);

    try {
        m_rDB.m_dbConnection->prepare("SessionUpdate_0", "UPDATE admin.khl_sessions "
            " SET user_id = $1, dtLast = $2, dtExpire = $3 "
            " WHERE id = $4"
        );
        m_rDB.m_dbConnection->prepare("SessionUpdate_1", "UPDATE admin.khl_users "
            " SET dtLast = $1, countConnect = countConnect + $2 "
            " WHERE id = $3 "
        );
        m_rDB.m_dbConnection->prepare("SessionUpdate_2", "UPDATE admin.khl_sessions "
            " SET dtLast = $1, dtExpire = $2 "
            " WHERE id = $3"
        );

        for (auto sp : rLS_) {
            Network::S2HSession* pSession = reinterpret_cast<Network::S2HSession*>(sp.get());
            std::string sLastActivity = to_iso_string(pSession->GetLastActivity());
            std::string sExpired = to_iso_string(pSession->GetExpired());

            if (pSession->IsAuthenticate()) {
                txn.prepared("SessionUpdate_0")(pSession->GetUserID())(sLastActivity)(sExpired)(pSession->GetSessionID()).exec();
                txn.prepared("SessionUpdate_1")(sLastActivity)(pSession->GetCountUse())(pSession->GetUserID()).exec();
            } else {
                txn.prepared("SessionUpdate_2")(sLastActivity)(sExpired)(pSession->GetSessionID()).exec();
            }
            session_ip_update(pSession, txn);
            pSession->ResetCountUse();
        }

        txn.commit();

        m_rDB.m_dbConnection->unprepare("SessionUpdate_0");
        m_rDB.m_dbConnection->unprepare("SessionUpdate_1");
        m_rDB.m_dbConnection->unprepare("SessionUpdate_2");
    } catch (const pqxx::pqxx_exception &e) {
//        OutputDebugString(e.base().what());
        std::cerr << e.base().what() << std::endl;
        const pqxx::sql_error *s = dynamic_cast<const pqxx::sql_error*>(&e.base());
        if (s) {
//            OutputDebugString(s->query().c_str());
            std::cerr << "Query was: " << s->query() << std::endl;
        }
    }

    return true;
}

bool S2HBStorage::SessionUpdate(S2HSession* pSession_) {
    pqxx::work txn(*m_rDB.m_dbConnection);

    std::string sLastActivity = to_iso_string(pSession_->GetLastActivity());
    std::string sCreated = to_iso_string(pSession_->GetCreated());
    std::string sExpired = to_iso_string(pSession_->GetExpired());

    if (pSession_->GetUserID() != 0) {
        std::string sUserID = std::to_string(pSession_->GetUserID());
        pqxx::result r = txn.exec(
            "SELECT dtFirst "
            "FROM admin.khl_users "
            "WHERE id = " + sUserID);

        if (r.size() > 0) {
            if (r[0][0].is_null()) {
                txn.exec(
                    "UPDATE admin.khl_users SET dtFirst = TIMESTAMP \'" + sCreated + "\', dtLast = TIMESTAMP \'" + sLastActivity + "\', "
                    " countConnect = 1 WHERE id = " + sUserID);
            } else {
                // если первое подключение было не в рамках текущей сессии
                txn.exec(
                    "UPDATE admin.khl_users SET dtLast = TIMESTAMP \'" + sLastActivity + "\', "
                    " countConnect = countConnect + " + std::to_string(pSession_->GetCountUse()) + " WHERE id = " + sUserID);
                pSession_->ResetCountUse();
            }
        }
        txn.exec(
            "UPDATE admin.khl_sessions "
            " SET user_id = " + sUserID + " , dtLast = TIMESTAMP '" + sLastActivity + "', dtExpire = TIMESTAMP '" + sExpired + "' "
            " WHERE id = '" + pSession_->GetSessionID() + "'");
    } else {
        txn.exec(
            "UPDATE admin.khl_sessions "
            " SET dtLast = TIMESTAMP '" + sLastActivity + "', dtExpire = TIMESTAMP '" + sExpired + "'"
            " WHERE id = '" + pSession_->GetSessionID() + "'");
    }

    session_ip_update(pSession_, txn);

    txn.commit();

    return true;
}

bool S2HBStorage::SessionLogger(const S2HSession* pSession_, const Json::Value& jsStat_) {
    pqxx::work txn(*m_rDB.m_dbConnection);

    txn.exec(
        "INSERT INTO admin.khl_sessions (id, dtFirst, dtLast, dtExpire, stats) "
        " VALUES ('" + pSession_->GetSessionID() + "', TIMESTAMP '" + to_iso_string(pSession_->GetCreated()) + "' , TIMESTAMP '" + to_iso_string(pSession_->GetLastActivity()) + "' , TIMESTAMP '" + to_iso_string(pSession_->GetExpired()) + "' , '" + Json::FastWriter().write(jsStat_) + "')");

    txn.commit();

    return true;
}

bool S2HBStorage::IsUserExist(const std::string& sLogin_) {
    pqxx::read_transaction txn(*m_rDB.m_dbConnection);

    pqxx::result r = txn.exec(
        "SELECT id "
        "FROM admin.khl_users "
        "WHERE login = " + txn.quote(sLogin_)
    );
    return r.size() > 0;
}

bool S2HBStorage::GetUserRoles(int& nUserID_, S2HSession* ps_) {
    pqxx::read_transaction txn(*m_rDB.m_dbConnection);

    ps_->ResetRoles();

    char    sSQL[2048];
    sprintf(sSQL, "SELECT r.code "
        " FROM admin.khl_user_roles AS ur "
        " INNER JOIN admin.khl_roles AS r ON (ur.role_id = r.id) "
        " WHERE ur.user_id = %d", nUserID_);

    pqxx::result r = txn.exec(sSQL);

    if (r.size() != 0) {
        for (auto row : r) {
            ps_->AppendRole(row[0].as<std::string>());
        }
        return true;
    } else {
        return false;
    }
}

bool S2HBStorage::GetUserInfo(int nUserID_, std::string& sLogin_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_) {
    pqxx::read_transaction txn(*m_rDB.m_dbConnection);

    pqxx::result r = txn.exec(
        "SELECT login, nickname, password, salt "
        "FROM admin.khl_users "
        "WHERE id = " + std::to_string(nUserID_)
    );

    if (r.size() == 0) {
        return false;
    }

    sLogin_ = r[0][0].as<std::string>();
    sNickname_ = r[0][1].as<std::string>();
    sPWHash_ = r[0][2].as<std::string>();
    sSalt_ = r[0][3].as<std::string>();

    return true;
}

bool S2HBStorage::GetUserInfo(const std::string& sLogin_, int& nUserID_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_) {
    pqxx::read_transaction txn(*m_rDB.m_dbConnection);

    pqxx::result r = txn.exec(
        "SELECT id, nickname, password, salt "
        "FROM admin.khl_users "
        "WHERE login = " + txn.quote(sLogin_)
    );

    if (r.size() == 0) {
        return false;
    }

    nUserID_ = r[0][0].as<int>();
    sNickname_ = r[0][1].as<std::string>();
    sPWHash_ = r[0][2].as<std::string>();
    sSalt_ = r[0][3].as<std::string>();

    return true;
}

void RecalcPasswordHash(std::string& sPwHash_, const std::string& sLogin_, const std::string& sPassword_, const std::string& sSalt_);

bool S2HBStorage::CreateUser(Json::Value& jsUser_) {
    pqxx::work txn(*m_rDB.m_dbConnection);

    std::string sLogin = jsUser_["Login"].asString();
    std::string sPassword = jsUser_["Password"].asString();

    boost::uuids::random_generator gen;
    boost::uuids::uuid  suid = gen();
    std::string sPwhash, sSalt = boost::lexical_cast<std::string>(suid);

    RecalcPasswordHash(sPwhash, sLogin, sPassword, sSalt);

    pqxx::result r = txn.exec(
        "INSERT INTO admin.khl_users ( "
        " login, nickname, password, salt) "
        " VALUES(" + txn.quote(sLogin) + " , " + txn.quote(jsUser_["Nickname"].asString()) + " , " + txn.quote(sPwhash) + " , " + txn.quote(sSalt) + " )"
        " RETURNING id");

    if (r.size() > 0) {
        int nUserID = r[0][0].as<int>();

        char    sSQL[2048];
        sprintf(sSQL, "INSERT INTO admin.khl_user_roles (user_id, role_id) "
            " SELECT %d, id FROM admin.khl_roles AS r WHERE r.code = $1", nUserID);

        m_rDB.m_dbConnection->prepare("InsertRole_0", sSQL);

        Json::Value jsRoles = jsUser_["Role"];
        for (Json::Value::ArrayIndex k = 0; k < jsRoles.size(); ++k) {
            Json::Value jsR = jsRoles[k];
            txn.prepared("InsertRole_0")(jsR.asString()).exec();
        }

        m_rDB.m_dbConnection->unprepare("InsertRole_0");
    }

    txn.commit();

    return true;
}

bool S2HBStorage::UpdatePassword(int nUserID_, const std::string& sPasswordHash_) {
    pqxx::work txn(*m_rDB.m_dbConnection);

    txn.exec(
        "UPDATE admin.khl_users "
        " SET password =  " + txn.quote(sPasswordHash_) +
        " WHERE id = " + std::to_string(nUserID_));

    txn.commit();

    return true;
}
