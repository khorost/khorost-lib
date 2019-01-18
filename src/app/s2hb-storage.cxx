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
#include "app/khl-define.h"

using namespace khorost::network;
using namespace khorost::db;

static void session_ip_update_impl(s2h_session* httpSession_, pqxx::work& txn_) {
    std::list<std::string>  lIPs;
    httpSession_->get_ip(lIPs, true);
    for (auto it : lIPs) {
        txn_.exec(
            "INSERT INTO admin.khl_sessions_ip (id, ip) "
            " VALUES('" + httpSession_->get_session_id() + "', '" + it + "' )"
            "  ON CONFLICT (id, ip) DO NOTHING "
            );
    }
}

void khorost::db::S2HBStorage::session_ip_update() {
    db_connection            conn(m_db_);
    pqxx::work              txn(conn.get_handle());

    pqxx::result r = txn.exec(
        "SELECT DISTINCT ip "
        " FROM admin.khl_sessions_ip "
        " WHERE host is NULL " 
        " LIMIT 10"
        );

    if (!r.empty()) {
        auto logger = spdlog::get(KHL_LOGGER_COMMON);
        
        for (const auto& row : r) {
            struct sockaddr_in saGNI = { 0 };
            char    hostname[1024] = "NULL", servInfo[1024];

            saGNI.sin_family = AF_INET;

            auto sIP = row[0].as<std::string>();

            logger->debug("[IP_UPDATE] check('{}')",sIP);

            inet_pton(AF_INET, sIP.c_str(), &saGNI.sin_addr);
            if (getnameinfo((struct sockaddr *)&saGNI, sizeof(struct sockaddr), hostname, sizeof(hostname), servInfo, sizeof(servInfo), NI_NUMERICSERV) == 0) {
                logger->debug("[IP_UPDATE] getnameinfo('{}') = '{}'", sIP, hostname);
                txn.exec(
                    "UPDATE admin.khl_sessions_ip "
                    " SET host = '" + std::string(hostname) + "' "
                    " WHERE ip = '" + sIP + "' AND host is NULL ");
            } else {
                logger->debug("[IP_UPDATE] getnameinfo('{}') failed", sIP);
                txn.exec(
                    "UPDATE admin.khl_sessions_ip "
                    " SET host = 'n/a' "
                    " WHERE ip = '" + sIP + "' AND host is NULL ");
            }
        }
        txn.commit();
    }
}

bool S2HBStorage::SessionUpdate(khorost::network::list_session& rLS_) {
    db_connection            conn(m_db_);
    pqxx::connection&       rcn = conn.get_handle();
    pqxx::work              txn(rcn);

    try {
        rcn.prepare("SessionUpdate_0", "UPDATE admin.khl_sessions "
            " SET user_id = $1, dtLast = $2, dtExpire = $3 "
            " WHERE id = $4"
        );
        rcn.prepare("SessionUpdate_1", "UPDATE admin.khl_users "
            " SET dtLast = $1, countConnect = countConnect + $2 "
            " WHERE id = $3 "
        );
        rcn.prepare("SessionUpdate_2", "UPDATE admin.khl_sessions "
            " SET dtLast = $1, dtExpire = $2 "
            " WHERE id = $3"
        );

        for (auto sp : rLS_) {
            s2h_session* pSession = reinterpret_cast<network::s2h_session*>(sp.get());
            std::string sLastActivity = to_iso_string(pSession->get_last_activity());
            std::string sExpired = to_iso_string(pSession->get_expired());

            if (pSession->IsAuthenticate()) {
                txn.prepared("SessionUpdate_0")(pSession->GetUserID())(sLastActivity)(sExpired)(pSession->get_session_id()).exec();
                txn.prepared("SessionUpdate_1")(sLastActivity)(pSession->GetCountUse())(pSession->GetUserID()).exec();
            } else {
                txn.prepared("SessionUpdate_2")(sLastActivity)(sExpired)(pSession->get_session_id()).exec();
            }
            session_ip_update_impl(pSession, txn);
            pSession->ResetCountUse();
        }

        txn.commit();

        rcn.unprepare("SessionUpdate_0");
        rcn.unprepare("SessionUpdate_1");
        rcn.unprepare("SessionUpdate_2");
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

bool S2HBStorage::SessionUpdate(s2h_session* pSession_) {
    db_connection            conn(m_db_);
    pqxx::work              txn(conn.get_handle());

    std::string sLastActivity = to_iso_string(pSession_->get_last_activity());
    std::string sCreated = to_iso_string(pSession_->get_created());
    std::string sExpired = to_iso_string(pSession_->get_expired());

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
            " WHERE id = '" + pSession_->get_session_id() + "'");
    } else {
        txn.exec(
            "UPDATE admin.khl_sessions "
            " SET dtLast = TIMESTAMP '" + sLastActivity + "', dtExpire = TIMESTAMP '" + sExpired + "'"
            " WHERE id = '" + pSession_->get_session_id() + "'");
    }

    session_ip_update_impl(pSession_, txn);

    txn.commit();

    return true;
}

bool S2HBStorage::SessionLogger(const s2h_session* pSession_, const Json::Value& jsStat_) {
    db_connection            conn(m_db_);
    pqxx::work              txn(conn.get_handle());

    txn.exec(
        "INSERT INTO admin.khl_sessions (id, dtFirst, dtLast, dtExpire, stats) "
        " VALUES ('" + pSession_->get_session_id() + "', TIMESTAMP '" + to_iso_string(pSession_->get_created()) + "' , TIMESTAMP '" + to_iso_string(pSession_->get_last_activity()) + "' , TIMESTAMP '" + to_iso_string(pSession_->get_expired()) + "' , " + to_string(txn,jsStat_) + ")");

    txn.commit();

    return true;
}

bool S2HBStorage::IsUserExist(const std::string& sLogin_) {
    db_connection            conn(m_db_);
    pqxx::nontransaction    txn(conn.get_handle());

    pqxx::result r = txn.exec(
        "SELECT id "
        "FROM admin.khl_users "
        "WHERE login = " + txn.quote(sLogin_)
    );
    return !r.empty();
}

bool S2HBStorage::GetUserRoles(int& nUserID_, s2h_session* ps_) {
    db_connection            conn(m_db_);
    pqxx::read_transaction  txn(conn.get_handle());

    ps_->ResetRoles();

    char    sSQL[2048];
    sprintf(sSQL, "SELECT r.code "
        " FROM admin.khl_user_roles AS ur "
        " INNER JOIN admin.khl_roles AS r ON (ur.role_id = r.id) "
        " WHERE ur.user_id = %d", nUserID_);

    pqxx::result r = txn.exec(sSQL);

    if (!r.empty()) {
        for (auto row : r) {
            ps_->AppendRole(row[0].as<std::string>());
        }
        return true;
    } else {
        return false;
    }
}

bool S2HBStorage::GetUserInfo(int nUserID_, std::string& sLogin_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_) {
    db_connection            conn(m_db_);
    pqxx::read_transaction  txn(conn.get_handle());

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

bool S2HBStorage::get_user_info(const std::string& login, int& user_id, std::string& nickname, std::string& password_hash, std::string& salt) const {
    db_connection            conn(m_db_);
    pqxx::read_transaction  txn(conn.get_handle());

    auto r = txn.exec(
        "SELECT id, nickname, password, salt "
        "FROM admin.khl_users "
        "WHERE login = " + txn.quote(login)
    );

    if (!r.empty()) {
        user_id = r[0][0].as<int>();
        nickname = r[0][1].as<std::string>();
        password_hash = r[0][2].as<std::string>();
        salt = r[0][3].as<std::string>();

        return true;
    }

    return false;
}

void RecalcPasswordHash(std::string& sPwHash_, const std::string& sLogin_, const std::string& sPassword_, const std::string& sSalt_);

bool S2HBStorage::CreateUser(Json::Value& jsUser_) {
    db_connection            conn(m_db_);
    pqxx::connection&       rcn = conn.get_handle();
    pqxx::work              txn(rcn);

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

        rcn.prepare("InsertRole_0", sSQL);

        Json::Value jsRoles = jsUser_["Role"];
        for (Json::Value::ArrayIndex k = 0; k < jsRoles.size(); ++k) {
            Json::Value jsR = jsRoles[k];
            txn.prepared("InsertRole_0")(jsR.asString()).exec();
        }

        rcn.unprepare("InsertRole_0");
    }

    txn.commit();

    return true;
}

bool S2HBStorage::UpdatePassword(int nUserID_, const std::string& sPasswordHash_) {
    db_connection            conn(m_db_);
    pqxx::work              txn(conn.get_handle());

    txn.exec(
        "UPDATE admin.khl_users "
        " SET password =  " + txn.quote(sPasswordHash_) +
        " WHERE id = " + std::to_string(nUserID_));

    txn.commit();

    return true;
}
