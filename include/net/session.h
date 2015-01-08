#pragma once
#ifndef _SESSION__H_
#define _SESSION__H_

#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

#include "db/sqlite3.h"

namespace Network {
    class Session {
        std::string m_sSessionID;
        time_t      m_dtCreated;
        time_t      m_dtExpired;
        time_t      m_dtLastActivity;
    public:
        Session(const std::string& sSessionID_, time_t dtCreated, time_t dtExpired);
        virtual ~Session();

        const std::string&  GetSessionID() const {return m_sSessionID;}
        time_t              GetCreated() const {return m_dtCreated;}
        time_t              GetExpired() const {return m_dtExpired;}

        void                SetExpired(time_t t_) {m_dtExpired = t_;}

        time_t              GetLastActivity() const {return m_dtLastActivity;}
        void                SetLastActivity(time_t t_) {m_dtLastActivity = t_;}

        virtual bool        ExportData(std::string& sData_) {return true;}
        virtual bool        ImportData(const std::string& sData_) {return true;}

        static std::string GenerateSessionID();
    };

    typedef	boost::shared_ptr<Session>	        SessionPtr;
    typedef std::map<std::string, SessionPtr>	DictSession;

    class SessionControler {
        class SessionDB : public DB::SQLite3 {
        public:
            SessionDB(){}
            virtual bool PrepareDatabase();

            bool    UpdateSession(SessionPtr sp_, int nVersion_);
            bool    RemoveSession(SessionPtr sp_);
            bool    LoadSessions(SessionControler& cs_);
        };

        SessionDB       m_SessionDB;
        DictSession     m_Sessions;
        int             m_nVersionMin, m_nVersionCurrent;

        virtual SessionPtr  CreateSession(const std::string& sSessionID_, time_t dtCreated_, time_t dtExpired_) { return SessionPtr(new Session(sSessionID_, dtCreated_, dtExpired_));}
    public:
        SessionControler();
        ~SessionControler();

        bool    Open(const std::string& sDriver_, int nVersionMin_, int nVersionCurrent_);

        SessionPtr  FindSession(const std::string& sSession_);
        SessionPtr  GetSession(const std::string& sSession_);
        bool        UpdateSession(SessionPtr sp_);

        int         GetVersionMin() {return m_nVersionMin;}
    };
}

#endif // _SESSION__H_
