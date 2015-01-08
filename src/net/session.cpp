#include "net/session.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

using namespace Network;

std::string Session::GenerateSessionID(){
    boost::uuids::uuid  uid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uid);
}

Session::Session(const std::string& sSessionID_, time_t dtCreated, time_t dtExpired){
    m_sSessionID    = sSessionID_;
    m_dtCreated     = dtCreated;
    m_dtExpired     = dtExpired;
    m_dtLastActivity= dtCreated;
}

Session::~Session(){
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

bool SessionControler::SessionDB::UpdateSession(SessionPtr sp_, int nVersion_) {
    Inserter Stmt(GetDB());
    
    bool bSuccess = Stmt.Prepare("INSERT OR REPLACE INTO sessions (SID, Version, dtCreate, dtUpdate, dtExpire, Data) VALUES(?,?,?,?,?,?)");
    
    Stmt.BindParam(1, sp_->GetSessionID());
    Stmt.BindParam(2, nVersion_);
    Stmt.BindParam(3, static_cast<int>(sp_->GetCreated()));
    Stmt.BindParam(4, static_cast<int>(time(NULL)));
    Stmt.BindParam(5, static_cast<int>(sp_->GetExpired()));

    std::string se;
    sp_->ExportData(se);
    Stmt.BindParam(6, se);

    bSuccess = Stmt.Exec();

    return bSuccess;
}

bool SessionControler::SessionDB::RemoveSession(SessionPtr sp_) {
    Inserter Stmt(GetDB());
    
    bool bSuccess = Stmt.Prepare("DELETE FROM sessions WHERE SID = ?");
    
    Stmt.BindParam(1, sp_->GetSessionID());

    bSuccess = Stmt.Exec();
    return bSuccess;
}

bool SessionControler::SessionDB::LoadSessions(SessionControler& cs_) {
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
            SessionPtr sp = cs_.CreateSession(s, dtc, dte);

            rStmt.GetValue(4, s);
            sp->ImportData(s);

            cs_.m_Sessions.insert(std::pair<std::string, SessionPtr>(sp->GetSessionID(), sp));
		} while(rStmt.MoveNext());
    }

    return true;
}

SessionPtr SessionControler::FindSession(const std::string& sSession_) {
    static SessionPtr spNull = SessionPtr();
    DictSession::iterator it = m_Sessions.find(sSession_);
    return it!=m_Sessions.end()?it->second:spNull;
}

SessionPtr SessionControler::GetSession(const std::string& sSession_) {
    time_t  t = time(NULL);
    DictSession::iterator it = m_Sessions.find(sSession_);

    if (it!=m_Sessions.end()) {
        SessionPtr  sp = it->second;
        if (sp->GetExpired()>t) {
            return sp;
        }
        m_Sessions.erase(it);
        m_SessionDB.RemoveSession(sp);
    }

    SessionPtr  sp = CreateSession(Session::GenerateSessionID(), t, t + 1209600);   // 2 недели = 1209600
                                                                                    // 1 час = 3600
    m_Sessions.insert(std::pair<std::string, SessionPtr>(sp->GetSessionID(), sp));
    m_SessionDB.UpdateSession(sp, m_nVersionCurrent);
    return sp;
}

bool SessionControler::UpdateSession(SessionPtr sp_) {
    return m_SessionDB.UpdateSession(sp_, m_nVersionCurrent);
}
