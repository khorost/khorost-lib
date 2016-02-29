#pragma once
#ifndef _SESSION__H_
#define _SESSION__H_

#include <map>
#include <list>
#include <string>
#include <boost/shared_ptr.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include "db/sqlite3.h"

namespace Network {
    typedef std::map<std::string, bool>	DictIP;
    class Session {

        std::string                 m_sSessionID;
        boost::posix_time::ptime    m_dtCreated;
        boost::posix_time::ptime    m_dtExpired;
        boost::posix_time::ptime    m_dtLastActivity;

        int                         m_nCount;
        DictIP                      m_IPs;
        bool                        m_bStatsUpdate;
    public:
        Session(const std::string& sSessionID_, boost::posix_time::ptime dtCreated_, boost::posix_time::ptime dtExpired_);
        virtual ~Session();

        const std::string&          GetSessionID() const {return m_sSessionID;}

        boost::posix_time::ptime    GetCreated() const { return m_dtCreated; }

        boost::posix_time::ptime    GetExpired() const { return m_dtExpired; }
        void                        SetExpired(boost::posix_time::ptime t_) { m_dtExpired = t_; }

        boost::posix_time::ptime    GetLastActivity() const { return m_dtLastActivity; }
        void                        SetLastActivity(boost::posix_time::ptime t_);

        int                         GetCountUse() const { return m_nCount; }
        void                        ResetCountUse() { m_nCount = 0; }
        void                        SetCountUse(int nCount_) { m_nCount = nCount_; }

        void                        SetIP(const std::string& sIP_);
        void                        GetIP(std::list<std::string>& lIPs_, bool bReset);

        bool                        IsStatsUpdate(bool bReset_) { 
            bool b = m_bStatsUpdate; 
            if (bReset_)  {
                m_bStatsUpdate = false;
            }
            return b; 
        }

        virtual bool                ExportData(std::string& sData_) {return true;}
        virtual bool                ImportData(const std::string& sData_) { return true; }

        static std::string GenerateSessionID();
    };

    typedef	boost::shared_ptr<Session>	        SessionPtr;
    typedef std::map<std::string, SessionPtr>	DictSession;
    typedef std::list<SessionPtr>	            ListSession;

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

        virtual SessionPtr  CreateSession(const std::string& sSessionID_, boost::posix_time::ptime dtCreated_, boost::posix_time::ptime dtExpired_) { 
            return SessionPtr(new Session(sSessionID_, dtCreated_, dtExpired_)); 
        }
    public:
        SessionControler();
        ~SessionControler();

        bool    Open(const std::string& sDriver_, int nVersionMin_, int nVersionCurrent_);

        SessionPtr  FindSession(const std::string& sSession_);
        SessionPtr  GetSession(const std::string& sSession_, bool& bCreate_);
        bool        UpdateSession(SessionPtr sp_);
        void        RemoveSession(SessionPtr sp_);

        int         GetVersionMin() {return m_nVersionMin;}
        bool        GetActiveSessionsStats(ListSession& rLS_);
    };
}

#endif // _SESSION__H_
