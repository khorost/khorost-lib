#pragma once

#include <map>
#include <list>
#include <string>
#include "boost/date_time/posix_time/posix_time.hpp"

#include "db/sqlite3.h"

namespace khorost {
    namespace network {
        typedef std::map<std::string, bool>	dict_ip;

        class session {

            std::string                 m_session_id;
            boost::posix_time::ptime    m_created;
            boost::posix_time::ptime    m_expired;
            boost::posix_time::ptime    m_last_activity;

            int                         m_count;
            dict_ip                      m_ips;
            bool                        m_stats_update;
        public:
            session(const std::string& session_id, const boost::posix_time::ptime created, const boost::posix_time::ptime expired);
            virtual ~session() = default;

            virtual void reset();

            const std::string&          get_session_id() const { return m_session_id; }

            boost::posix_time::ptime    get_created() const { return m_created; }

            boost::posix_time::ptime    get_expired() const { return m_expired; }
            void                        set_expired(const boost::posix_time::ptime value) { m_expired = value; }

            boost::posix_time::ptime    get_last_activity() const { return m_last_activity; }
            void                        set_last_activity(const boost::posix_time::ptime value);

            int                         GetCountUse() const { return m_count; }
            void                        ResetCountUse() { m_count = 0; }
            void                        set_count_use(int nCount_) { m_count = nCount_; }

            void                        set_ip(const std::string& ip);
            void                        get_ip(std::list<std::string>& ips, bool reset);

            bool                        IsStatsUpdate(bool bReset_) {
                bool b = m_stats_update;
                if (bReset_)  {
                    m_stats_update = false;
                }
                return b;
            }

            virtual void                get_expire_shift(boost::posix_time::time_duration& value) { value = boost::posix_time::minutes(DEFAULT_EXPIRE_MINUTES); }
            virtual bool                export_data(std::string& data) { return true; }
            virtual bool                import_data(const std::string& data) { return true; }

            static const int DEFAULT_EXPIRE_MINUTES;
            static std::string generate_session_id();
        };

        typedef	std::shared_ptr<session>	        session_ptr;
        typedef std::map<std::string, session_ptr>	dict_session;
        typedef std::list<session_ptr>	            list_session;

        class session_controller {
            class session_db final : public db::khl_sqlite3 {
            public:
                session_db() = default;

                bool PrepareDatabase() override;

                bool update_session(session* session, int version);
                bool remove_session(session* session);
                bool load_sessions(session_controller& session_controler, const std::function<session_ptr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator);
            };

            session_db m_session_db_;
            dict_session m_session_memory_;
            int m_version_min_, m_version_current_;

        public:
            session_controller();
            virtual ~session_controller() = default;

            bool open(const std::string& driver, int version_min, int version_current, const std::function<session_ptr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator);

            session_ptr find_session(const std::string& session_id);
            session_ptr get_session(const std::string& session_id, bool& created, std::function<session_ptr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator );

            bool update_session(session* sp_);
            void remove_session(session* sp_);

            int get_version_min() const { return m_version_min_; }
            bool get_active_sessions_stats(list_session& rLS_);
            void check_alive_sessions();
        };
    }
}
