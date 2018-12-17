﻿#ifndef __S2HBSTORAGE_H__
#define __S2HBSTORAGE_H__

#include <string>
#include <list>
#include <unordered_map>

#include <json/json.h>

#include "db/postgres.h"
#include "app/s2h-session.h"

namespace khorost {
    namespace db {
        class S2HBStorage : public khorost::db::khl_postgres {
        public:
            S2HBStorage(postgres& db) : khorost::db::khl_postgres(db) {
            }

            bool    SessionLogger(const network::s2h_session* pSession_, const Json::Value& jsStat_);
            bool    SessionUpdate(network::s2h_session* pSession_);
            bool    SessionUpdate(khorost::network::list_session& rLS_);
            void    SessionIPUpdate();

            bool    CreateUser(Json::Value& jsUser_);
            bool    UpdatePassword(int nUserID_, const std::string& sPasswordHash_);

            bool    IsUserExist(const std::string& sLogin_);
            bool    get_user_info(const std::string& sLogin_, int& nUserID_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_) const;
            bool    GetUserInfo(int nUserID_, std::string& sLogin_, std::string& sNickname_, std::string& sPWHash_, std::string& sSalt_);
            bool    GetUserRoles(int& nUserID_, network::s2h_session* ps_);
        };
    }
}

#endif // !__S2HBSTORAGE_H__
