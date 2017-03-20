#include "net/session.h"
#include "util/utils.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

using namespace khorost::Network;

const int Session::DEFAULT_EXPIRE_MINUTES = 30;

std::string Session::GenerateSessionID(){
    boost::uuids::uuid  uid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uid);
}

Session::Session(const std::string& sSessionID_, boost::posix_time::ptime dtCreated_, boost::posix_time::ptime dtExpired_) {
    m_sSessionID    = sSessionID_;
    m_dtCreated     = dtCreated_;
    m_dtExpired     = dtExpired_;
    m_dtLastActivity= dtCreated_;
 
    m_nCount = 0;
    m_bStatsUpdate = false;
}

Session::~Session(){
}

void Session::SetIP(const std::string& sIP_) {
    DictIP::iterator    it = m_IPs.find(sIP_);
    if (it == m_IPs.end()) {
        m_IPs.insert(std::pair<std::string, bool>(sIP_, false));
        m_bStatsUpdate = true;
    }
}

void Session::GetIP(std::list<std::string>& lIPs_, bool bReset_) {
    for (auto it : m_IPs) {
        if (!it.second) {
            lIPs_.push_back(it.first);
            if (bReset_) {
                it.second = true;
            }
        }
    }
}

void Session::SetLastActivity(boost::posix_time::ptime t_) {
    using namespace boost::posix_time;

    time_duration   td = t_ - m_dtLastActivity;
    if ( td.minutes() > 20) { // 20 минут неактивности
        m_nCount++;
    }

    m_dtLastActivity = t_;
    m_bStatsUpdate = true;
}

SessionControler::SessionControler() {
    m_nVersionMin = m_nVersionCurrent = 1;
}

SessionControler::~SessionControler() {

}

bool SessionControler::Open(const std::string& sDriver_, int nVersionMin_, int nVersionCurrent_) {
    m_nVersionMin = nVersionMin_;
    m_nVersionCurrent = nVersionCurrent_;

    m_SessionDB.Open(sDriver_);
    m_SessionDB.LoadSessions(*this);
    return true;
}

bool SessionControler::SessionDB::PrepareDatabase() {
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

bool SessionControler::SessionDB::UpdateSession(Session* sp_, int nVersion_) {
    using namespace boost::posix_time;

    Inserter Stmt(GetDB());
    
    bool bSuccess = Stmt.Prepare("INSERT OR REPLACE INTO sessions (SID, Version, dtCreate, dtUpdate, dtExpire, Data) VALUES(?,?,?,?,?,?)");
    
    Stmt.BindParam(1, sp_->GetSessionID());
    Stmt.BindParam(2, nVersion_);
    Stmt.BindParam(3, static_cast<int>(khorost::Data::EpochDiff(sp_->GetCreated()).total_seconds() ));
    Stmt.BindParam(4, static_cast<int>(time(NULL)));
    Stmt.BindParam(5, static_cast<int>(khorost::Data::EpochDiff(sp_->GetExpired()).total_seconds() ));

    std::string se;
    sp_->ExportData(se);
    Stmt.BindParam(6, se);

    bSuccess = Stmt.Exec();

    return bSuccess;
}

bool SessionControler::SessionDB::RemoveSession(Session* sp_) {
    Inserter Stmt(GetDB());
    
    bool bSuccess = Stmt.Prepare("DELETE FROM sessions WHERE SID = ?");
    
    Stmt.BindParam(1, sp_->GetSessionID());

    bSuccess = Stmt.Exec();
    return bSuccess;
}

bool SessionControler::SessionDB::LoadSessions(SessionControler& cs_) {
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    
    Reader      rStmt(GetDB());
    Inserter    iStmt(GetDB());
	
    iStmt.Prepare("DELETE FROM sessions WHERE dtExpire < ? OR Version < ? ");
    iStmt.BindParam(1, static_cast<int>(time(NULL)));
    iStmt.BindParam(2, cs_.GetVersionMin());

    bool bSuccess = iStmt.Exec();

	static unsigned int s_index = 1;
	unsigned int indexMVK = 0;

	rStmt.Prepare(
		"SELECT SID, dtCreate, dtExpire, Data "
		"FROM sessions ");

	bSuccess = rStmt.MoveFirst();
    if(bSuccess) {
        std::string s;
        int dtc, dte;
		do {
			rStmt.GetValue(1, s);
			rStmt.GetValue(2, dtc);
			rStmt.GetValue(3, dte);
            SessionPtr sp = cs_.CreateSession(s, from_time_t(dtc), from_time_t(dte));

            rStmt.GetValue(4, s);
            sp->ImportData(s);

            cs_.m_SessionMemory.insert(std::pair<std::string, SessionPtr>(sp->GetSessionID(), sp));
		} while(rStmt.MoveNext());
    }

    return true;
}

void SessionControler::CheckAliveSessions() {
    using namespace boost::posix_time;

    ptime                   ptNow = second_clock::universal_time();

    for (DictSession::iterator it = m_SessionMemory.begin(); it != m_SessionMemory.end();){
        DictSession::iterator sit = it++;
        SessionPtr  sp = sit->second;
        if (sp->GetExpired() < ptNow) {
            m_SessionMemory.erase(sit);
            m_SessionDB.RemoveSession(sp.get());
        }
    }
}

bool SessionControler::GetActiveSessionsStats(ListSession& rLS_) {
    for (auto s : m_SessionMemory){
        if (s.second->IsStatsUpdate(true) != 0) {
            rLS_.push_back(s.second);
        }
    }
    return rLS_.size() != 0;
}

SessionPtr SessionControler::FindSession(const std::string& sSession_) {
    static SessionPtr spNull = SessionPtr();
    DictSession::iterator it = m_SessionMemory.find(sSession_);
    return it!=m_SessionMemory.end()?it->second:spNull;
}

SessionPtr SessionControler::GetSession(const std::string& sSession_, bool& bCreate_) {
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    time_duration           td;
    ptime                   ptNow = second_clock::universal_time();
    DictSession::iterator   it = m_SessionMemory.find(sSession_);

    if (it!=m_SessionMemory.end()) {
        SessionPtr  sp = it->second;
        if (sp->GetExpired()>ptNow) {
            sp->GetExpireShift(td);
            sp->SetExpired(ptNow + td);

            bCreate_ = false;
            return sp;
        }
        m_SessionMemory.erase(it);
        m_SessionDB.RemoveSession(sp.get());
    }

    ptime  ptExpire = ptNow + minutes(Session::DEFAULT_EXPIRE_MINUTES);
    SessionPtr  sp = CreateSession(Session::GenerateSessionID(), ptNow, ptExpire);   // 2 недели = 1209600
                                                                                    // 1 час = 3600
    sp->SetCountUse(1);
    m_SessionMemory.insert(std::pair<std::string, SessionPtr>(sp->GetSessionID(), sp));
    m_SessionDB.UpdateSession(sp.get(), m_nVersionCurrent);
    bCreate_ = true;

    return sp;
}

bool SessionControler::UpdateSession(Session* sp_) {
    return m_SessionDB.UpdateSession(sp_, m_nVersionCurrent);
}

void SessionControler::RemoveSession(Session* sp_) {
    DictSession::iterator it = m_SessionMemory.find(sp_->GetSessionID());
    if (it != m_SessionMemory.end()) {
        m_SessionMemory.erase(it);
    }
    m_SessionDB.RemoveSession(sp_);
}
