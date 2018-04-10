#ifndef __POSTGRES_H__
#define __POSTGRES_H__

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

namespace khorost {
    namespace DB {
        class DBConnectionPool {
            boost::scoped_ptr<pqxx::connection>    m_spConnect;
        public:
            DBConnectionPool(const std::string& sConnectParam_) {
                Reconnect(sConnectParam_);
            }
            virtual ~DBConnectionPool() {
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

        typedef	boost::shared_ptr<DBConnectionPool>	DBConnectionPoolPtr;

        class DBPool {
            std::queue<DBConnectionPoolPtr>  m_FreePool;

            std::mutex m_mutex;
            std::condition_variable m_condition;

        public:
            DBPool() {
            }

            void Prepare(int nCount_, const std::string& sConnectParam_);

            DBConnectionPoolPtr GetConnectionPool();
            void    ReleaseConnectionPool(DBConnectionPoolPtr pdbc_);
        };

        class DBConnection {
            DBPool&             m_dbPool;
            DBConnectionPoolPtr m_pdbConnectionPool;
        public:
            DBConnection(DBPool& dbp_) : m_dbPool(dbp_), m_pdbConnectionPool(nullptr) {
                m_pdbConnectionPool = m_dbPool.GetConnectionPool();
            }
            virtual ~DBConnection() {
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


        class Postgres : public DBPool  {
            int         m_nPort;
            std::string m_sHost;
            std::string m_sDatabase;
            std::string m_sLogin;
            std::string m_sPassword;

        public:
            Postgres() {
            }
            virtual ~Postgres() {
            }

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

        class LinkedPostgres {
        protected:
            Postgres& m_rDB;
        public:
            LinkedPostgres(Postgres& rDB_) : m_rDB(rDB_) {}
            virtual ~LinkedPostgres() = default;

            static std::string to_string(const pqxx::transaction_base& txn, const boost::posix_time::ptime& timestamp, bool nullable_infinity = true);
            static std::string to_string(const pqxx::transaction_base& txn, const Json::Value& info);
        private:

        };
    }
}

#endif  // __POSTGRES_H__
