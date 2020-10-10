// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

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
