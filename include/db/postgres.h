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

#include "boost/date_time/posix_time/posix_time.hpp"

#include <json/json.h>

#include "net/token.h"
#include "util/i18n.h"
#include <spdlog/spdlog.h>

namespace khorost {
    namespace db {
        class db_connection_pool final {
            std::unique_ptr<pqxx::connection> m_connect_;
        public:
            explicit db_connection_pool(const std::string& connect_param) {
                reconnect(connect_param);
            }

            ~db_connection_pool() = default;

            pqxx::connection& get_handle() const {
                return *m_connect_;
            }

            void reconnect(const std::string& connect_param) {
                m_connect_.reset(new pqxx::connection(connect_param));
            }

            void disconnect() {
                m_connect_.reset(nullptr);
            }

            bool check_connect() const;
        };

        typedef std::shared_ptr<db_connection_pool> db_connection_pool_ptr;

        class db_pool {
            std::queue<db_connection_pool_ptr> m_free_pool_;

            std::mutex m_mutex_;
            std::condition_variable m_condition_;

        public:
            db_pool() = default;

            void prepare(int count, const std::string& connect_param);

            db_connection_pool_ptr get_connection_pool();
            void release_connection_pool(db_connection_pool_ptr pdbc);
        };

        class db_connection final {
            db_pool& m_db_pool_;
            db_connection_pool_ptr m_pdb_connection_pool_;
        public:
            explicit db_connection(db_pool& dbp) : m_db_pool_(dbp), m_pdb_connection_pool_(nullptr) {
                m_pdb_connection_pool_ = m_db_pool_.get_connection_pool();
            }

            ~db_connection() {
                release_connection_pool();
            }

            pqxx::connection& get_handle() const {
                return m_pdb_connection_pool_->get_handle();
            }

            void release_connection_pool() {
                if (m_pdb_connection_pool_ != nullptr) {
                    m_db_pool_.release_connection_pool(m_pdb_connection_pool_);
                    m_pdb_connection_pool_ = nullptr;
                }
            }
        };


        class postgres final : public db_pool {
            int m_port_;
            std::string m_host_;
            std::string m_database_;
            std::string m_login_;
            std::string m_password_;

        public:
            postgres() = default;
            ~postgres() = default;

            std::string get_connect_param() const;

            void set_connect(const std::string& host, int port, const std::string& database, const std::string& login, const std::string& password) {
                m_host_ = host;
                m_port_ = port;
                m_database_ = database;
                m_login_ = login;
                m_password_ = password;

                prepare(5, get_connect_param());
            }

            void execute_custom_sql(bool b_read_only, const std::string& sql, Json::Value& json_result);

        };

        class linked_postgres {
        protected:
            postgres& m_db_;
        public:
            explicit linked_postgres(postgres& db) : m_db_(db) {
            }

            virtual ~linked_postgres() = default;

            static std::string to_string(const pqxx::transaction_base& txn, const boost::posix_time::ptime& timestamp, bool nullable_infinity = true);
            static std::string to_string(const pqxx::transaction_base& txn, const Json::Value& info);
            static std::string to_string(const pqxx::transaction_base& txn, const bool value);
            static std::string to_string(const pqxx::transaction_base& txn, const unsigned int value);
        private:

        };

        class khl_postgres : protected linked_postgres {
        protected:
            internalization m_internalization_;
        public:
            explicit khl_postgres(postgres& db) : linked_postgres(db) {
            }

            virtual ~khl_postgres() = default;

            network::token_ptr create_token(int access_timeout, int refresh_timeout, int append_time, const Json::Value& payload) const;
            network::token_ptr load_token(bool is_access_token, const std::string& token_id) const;
            bool refresh_token(network::token_ptr& token, int access_timeout, int refresh_timeout, int append_time) const;

            internalization::func_append_value load_tag();

            std::string load_value_by_tag(const std::string& lang, const std::string& tag) const;

        };
    }
}
