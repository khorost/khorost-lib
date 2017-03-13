#ifndef __S2HBSTORAGE_H__
#define __S2HBSTORAGE_H__

#include <string>
#include <list>
#include <unordered_map>

#include <json/json.h>

#include "db/postgres.h"
#include "app/s2h-session.h"

namespace khorost {
    namespace DB {
        class S2HBStorage : public khorost::DB::LinkedPostgres {
        public:
            S2HBStorage(Postgres& rDB_) : khorost::DB::LinkedPostgres(rDB_) {
            }

            bool    SessionLogger(const Network::S2HSession* pSession_, const Json::Value& jsStat_);
            bool    SessionUpdate(Network::S2HSession* pSession_);
            bool    SessionUpdate(khorost::Network::ListSession& rLS_);
            void    SessionIPUpdate();

            bool    CreateUser(Json::Value& jsUser_);
            bool    UpdatePassword(int nUserID_, const std::string& sPasswordHash_);

            bool    IsUserExist(const std::string& sLogin_);
            bool    GetUserInfo(const std::string& sLogin_, int& nUserID_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_);
            bool    GetUserInfo(int nUserID_, std::string& sLogin_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_);
            bool    GetUserRoles(int& nUserID_, Network::S2HSession* ps_);
        };
    }
}

#endif // !__S2HBSTORAGE_H__
