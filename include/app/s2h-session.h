#ifndef __S2H_SESSION__
#define __S2H_SESSION__

#include <string>
#include <set>

#include "net/session.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <json/json.h>

#define SESSION_VERSION_MIN         4
#define SESSION_VERSION_CURRENT     4

namespace khorost {
    namespace Network {
        class S2HSession : public Session {
            bool		        m_bAuthenticate;
            int                 m_idUser;
            std::string         m_sNickname;
            std::string         m_sPosition;
            std::set<std::string>   m_dRoles;
        public:
            S2HSession(const std::string& sSessionID_, boost::posix_time::ptime dtCreated, boost::posix_time::ptime dtExpired);
            virtual ~S2HSession() = default;

            virtual void        GetExpireShift(boost::posix_time::time_duration& td_) {
                if (m_bAuthenticate) {
                    td_ = boost::posix_time::hours(24 * 14);
                } else {
                    Session::GetExpireShift(td_);
                }
            }
            virtual bool        ExportData(std::string& sData_);
            virtual bool        ImportData(const std::string& sData_);

            void    reset();

            // ****************************************************************
            bool IsAuthenticate() const { return m_bAuthenticate; }
            void SetAuthenticate(bool bValue_){ m_bAuthenticate = bValue_; }

            void        SetUserID(int rID_) { m_idUser = rID_; }
            int         GetUserID() const { return m_idUser; }

            void                SetNickname(const std::string& sNickname_) { m_sNickname = sNickname_; }
            const std::string&  GetNickname() const { return m_sNickname; }

            void                SetPostion(const std::string& sPosition_) { m_sPosition = sPosition_; }
            const std::string&  GetPosition() const { return m_sPosition; }

            void        ResetRoles() { m_dRoles.empty(); }
            void        AppendRole(const std::string& sRole_) { m_dRoles.insert(sRole_); }
            bool        IsRoleExist(const std::string& sRole_) const { return m_dRoles.find(sRole_) != m_dRoles.end(); }
            void        fill_roles(Json::Value& value);
        };

        class S2HSessionController : public SessionControler {
            virtual SessionPtr  CreateSession(const std::string& sSessionID_, boost::posix_time::ptime dtCreated_, boost::posix_time::ptime dtExpired_) {
                return SessionPtr(new S2HSession(sSessionID_, dtCreated_, dtExpired_));
            }
        public:
            bool    Open(const std::string& sDriver_) {
                return SessionControler::Open(sDriver_, SESSION_VERSION_MIN, SESSION_VERSION_CURRENT);
            }
        };
    }
}

#endif  // __S2H_SESSION__
