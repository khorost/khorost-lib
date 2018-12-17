#pragma once

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>

#include <pqxx/pqxx>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <json/json.h>

#include "net/token.h"

namespace khorost {
    namespace db {
        class db_connection_pool {
            boost::scoped_ptr<pqxx::connection>    m_spConnect;
        public:
            db_connection_pool(const std::string& sConnectParam_) {
                Reconnect(sConnectParam_);
            }
            virtual ~db_connection_pool() {
            }

            pqxx::connection& GetHandle() {
                return *m_spConnect;
            }

            void    Reconnect(const std::string& sConnectParam_) {
                m_spConnect.reset(new pqxx::connection(sConnectParam_));
            }
            void    Disconnect() {
                m_spConnect.reset(nullptr);
            }
            bool    CheckConnect();
        };

        typedef	boost::shared_ptr<db_connection_pool>	db_connection_pool_ptr;

        class db_pool {
            std::queue<db_connection_pool_ptr>  m_FreePool;

            std::mutex m_mutex;
            std::condition_variable m_condition;

        public:
            db_pool() = default;

            void Prepare(int nCount_, const std::string& sConnectParam_);

            db_connection_pool_ptr GetConnectionPool();
            void    ReleaseConnectionPool(db_connection_pool_ptr pdbc_);
        };

        class db_connection {
            db_pool&             m_dbPool;
            db_connection_pool_ptr m_pdbConnectionPool;
        public:
            db_connection(db_pool& dbp_) : m_dbPool(dbp_), m_pdbConnectionPool(nullptr) {
                m_pdbConnectionPool = m_dbPool.GetConnectionPool();
            }
            virtual ~db_connection() {
                ReleaseConnectionPool();
            }

            pqxx::connection& GetHandle() {
                return m_pdbConnectionPool->GetHandle();
            }

            void    ReleaseConnectionPool() {
                if (m_pdbConnectionPool != nullptr) {
                    m_dbPool.ReleaseConnectionPool(m_pdbConnectionPool);
                    m_pdbConnectionPool = nullptr;
                }
            }
        };


        class postgres : public db_pool  {
            int         m_nPort;
            std::string m_sHost;
            std::string m_sDatabase;
            std::string m_sLogin;
            std::string m_sPassword;

        public:
            postgres() {
            }
            virtual ~postgres() = default;

            std::string GetConnectParam() const;

            void    SetConnect(std::string sHost_, int nPort_, std::string sDatabase_, std::string sLogin_, std::string sPassword_) {
                m_sHost = sHost_;
                m_nPort = nPort_;
                m_sDatabase = sDatabase_;
                m_sLogin = sLogin_;
                m_sPassword = sPassword_;

                Prepare(5, GetConnectParam());
            }

            void    ExecuteCustomSQL(bool bReadOnly_, const std::string& sSQL_, Json::Value& jvResult_);

        };

        class linked_postgres {
        protected:
            postgres& m_rDB;
        public:
            linked_postgres(postgres& rDB_) : m_rDB(rDB_) {}
            virtual ~linked_postgres() = default;

            static std::string to_string(const pqxx::transaction_base& txn, const boost::posix_time::ptime& timestamp, bool nullable_infinity = true);
            static std::string to_string(const pqxx::transaction_base& txn, const Json::Value& info);
            static std::string to_string(const pqxx::transaction_base& txn, const bool value);
            static std::string to_string(const pqxx::transaction_base& txn, const unsigned int value);
        private:

        };

        class khl_postgres : protected linked_postgres {
        public:
            khl_postgres(postgres& db) : linked_postgres(db) {}
            virtual ~khl_postgres() = default;

            network::token_ptr create_token(int access_timeout, int refresh_timeout, const Json::Value& payload) const;
            network::token_ptr load_token(bool is_refresh_token, const std::string& token_id) const;
        };
    }
}
