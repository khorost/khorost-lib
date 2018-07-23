#include "net/session.h"
#include "util/utils.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

using namespace khorost::Network;

const int session::DEFAULT_EXPIRE_MINUTES = 30;

std::string session::generate_session_id() {
    const auto uid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uid);
}

session::session(const std::string& session_id, const boost::posix_time::ptime created, const boost::posix_time::ptime expired) {
    m_session_id = session_id;
    m_created = created;
    m_expired = expired;
    m_last_activity = created;

    reset();
}

void session::reset() {
    m_count = 0;
    m_stats_update = false;
}

void session::set_ip(const std::string& ip) {
    const auto it = m_ips.find(ip);
    if (it == m_ips.end()) {
        m_ips.insert(std::pair<std::string, bool>(ip, false));
        m_stats_update = true;
    }
}

void session::get_ip(std::list<std::string>& ips, bool reset) {
    for (auto& it : m_ips) {
        if (!it.second) {
            ips.push_back(it.first);
            if (reset) {
                it.second = true;
            }
        }
    }
}

void session::set_last_activity(const boost::posix_time::ptime value) {
    using namespace boost::posix_time;

    auto td = value - m_last_activity;
    if (td.minutes() > 20) {
        // 20 минут неактивности
        m_count++;
    }

    m_last_activity = value;
    m_stats_update = true;
}

session_controler::session_controler() {
    m_nVersionMin = m_nVersionCurrent = 1;
}

bool session_controler::open(const std::string& driver, int version_min, int version_current, const std::function<SessionPtr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator) {
    m_nVersionMin = version_min;
    m_nVersionCurrent = version_current;

    m_SessionDB.Open(driver);
    m_SessionDB.load_sessions(*this, creator);
    return true;
}

bool session_controler::session_db::PrepareDatabase() {
    if (!SQLite3::PrepareDatabase()) {
        return false;
    }

    ExecuteStmt("CREATE TABLE sessions ("
        "SID text, "
        "Version integer, "
        "dtCreate integer, "
        "dtUpdate integer, "
        "dtExpire integer, "
        "Data text, "
        "constraint pk_sessions primary key (SID) "
        ")");

    ExecuteStmt("CREATE INDEX idx_sessions_expire ON sessions ( "
        "	dtExpire"
        ")");

    return true;
}

bool session_controler::session_db::update_session(session* session, const int version) {
    using namespace boost::posix_time;

    Inserter stmt(GetDB());

    stmt.Prepare(
        "INSERT OR REPLACE INTO sessions (SID, Version, dtCreate, dtUpdate, dtExpire, Data) VALUES(?,?,?,?,?,?)");

    stmt.BindParam(1, session->get_session_id());
    stmt.BindParam(2, version);
    stmt.BindParam(3, static_cast<int>(Data::EpochDiff(session->get_created()).total_seconds()));
    stmt.BindParam(4, static_cast<int>(time(nullptr)));
    stmt.BindParam(5, static_cast<int>(Data::EpochDiff(session->get_expired()).total_seconds()));

    std::string data;
    session->export_data(data);
    stmt.BindParam(6, data);

    return stmt.Exec();
}

bool session_controler::session_db::remove_session(session* session) {
    Inserter stmt(GetDB());
    
    stmt.Prepare("DELETE FROM sessions WHERE SID = ?");
    stmt.BindParam(1, session->get_session_id());

    return stmt.Exec();
}

bool session_controler::session_db::load_sessions(session_controler& session_controler, const std::function<SessionPtr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator) {
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    Reader r_stmt(GetDB());
    Inserter i_stmt(GetDB());

    i_stmt.Prepare("DELETE FROM sessions WHERE dtExpire < ? OR Version < ? ");
    i_stmt.BindParam(1, static_cast<int>(time(nullptr)));
    i_stmt.BindParam(2, session_controler.GetVersionMin());

    i_stmt.Exec();

    r_stmt.Prepare(
        "SELECT SID, dtCreate, dtExpire, Data "
        "FROM sessions ");

    if (r_stmt.MoveFirst()) {
        std::string s;
        int dtc, dte;
        do {
            r_stmt.GetValue(1, s);
            r_stmt.GetValue(2, dtc);
            r_stmt.GetValue(3, dte);

            if (creator != nullptr) {
                auto sp = creator(s, from_time_t(dtc), from_time_t(dte));

                r_stmt.GetValue(4, s);
                sp->import_data(s);

                session_controler.m_SessionMemory.insert(std::pair<std::string, SessionPtr>(sp->get_session_id(), sp));
            }
        } while (r_stmt.MoveNext());
    }

    return true;
}

void session_controler::CheckAliveSessions() {
    using namespace boost::posix_time;

    const auto now = second_clock::universal_time();

    for (auto it = m_SessionMemory.begin(); it != m_SessionMemory.end();){
        const auto sit = it++;
        SessionPtr  sp = sit->second;
        if (sp->get_expired() < now) {
            m_SessionDB.remove_session(sp.get());
            m_SessionMemory.erase(sit);
        }
    }
}

bool session_controler::GetActiveSessionsStats(ListSession& rLS_) {
    for (auto s : m_SessionMemory){
        if (s.second->IsStatsUpdate(true) != 0) {
            rLS_.push_back(s.second);
        }
    }
    return rLS_.size() != 0;
}

SessionPtr session_controler::find_session(const std::string& session_id) {
    const auto it = m_SessionMemory.find(session_id);
    return it != m_SessionMemory.end() ? it->second : nullptr;
}

SessionPtr session_controler::get_session(const std::string& session_id, bool& created, const std::function<SessionPtr(const std::string& session_id, boost::posix_time::ptime created, boost::posix_time::ptime expired)> creator) {
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    time_duration td;
    const auto now = second_clock::universal_time();
    const auto it = m_SessionMemory.find(session_id);

    if (it != m_SessionMemory.end()) {
        auto sp = it->second;
        if (sp->get_expired() > now) {
            sp->get_expire_shift(td);
            sp->set_expired(now + td);

            created = false;
            return sp;
        }

        m_SessionMemory.erase(it);
        m_SessionDB.remove_session(sp.get());
    }

    const auto expire = now + minutes(session::DEFAULT_EXPIRE_MINUTES);
    if (creator != nullptr) {
        auto sp = creator(session::generate_session_id(), now, expire); // 2 недели = 1209600
        // 1 час = 3600
        sp->set_count_use(1);
        m_SessionMemory.insert(std::pair<std::string, SessionPtr>(sp->get_session_id(), sp));
        m_SessionDB.update_session(sp.get(), m_nVersionCurrent);
        created = true;
        return sp;
    }

    return nullptr;
}

bool session_controler::update_session(session* session) {
    return m_SessionDB.update_session(session, m_nVersionCurrent);
}

void session_controler::remove_session(session* session) {
    m_SessionDB.remove_session(session);

    const auto it = m_SessionMemory.find(session->get_session_id());
    if (it != m_SessionMemory.end()) {
        m_SessionMemory.erase(it);
    }
}
