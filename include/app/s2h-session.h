#pragma once

#include <string>
#include <set>

#include "net/session.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <json/json.h>

#define SESSION_VERSION_MIN         4
#define SESSION_VERSION_CURRENT     4

namespace khorost {
    namespace network {
        class s2h_session : public session {
            bool		        m_bAuthenticate;
            int                 m_idUser;
            std::string         m_sNickname;
            std::string         m_sPosition;
            std::set<std::string>   m_dRoles;
        public:
            s2h_session(const std::string& session_id, const boost::posix_time::ptime& created, const boost::posix_time::ptime& expired) :
                session(session_id, created, expired) {
                reset();
            }

            virtual ~s2h_session() = default;

            virtual void        get_expire_shift(boost::posix_time::time_duration& value) override;
            virtual bool        export_data(std::string& data) override;
            virtual bool        import_data(const std::string& data) override;

            virtual void    reset() override;

            // ****************************************************************
            bool IsAuthenticate() const { return m_bAuthenticate; }
            void SetAuthenticate(bool bValue_){ m_bAuthenticate = bValue_; }

            void        SetUserID(int rID_) { m_idUser = rID_; }
            int         GetUserID() const { return m_idUser; }

            void                SetNickname(const std::string& sNickname_) { m_sNickname = sNickname_; }
            const std::string&  GetNickname() const { return m_sNickname; }

            void                SetPostion(const std::string& sPosition_) { m_sPosition = sPosition_; }
            const std::string&  GetPosition() const { return m_sPosition; }

            void        ResetRoles() { m_dRoles.clear(); }
            void        AppendRole(const std::string& sRole_) { m_dRoles.insert(sRole_); }
            bool        IsRoleExist(const std::string& sRole_) const { return m_dRoles.find(sRole_) != m_dRoles.end(); }
            void        fill_roles(Json::Value& value);
        };

        /*
        class S2HSessionController : public session_controler {
            virtual SessionPtr  create_session(const std::string& sSessionID_, boost::posix_time::ptime dtCreated_, boost::posix_time::ptime dtExpired_) override {
                return SessionPtr(new S2HSession(sSessionID_, dtCreated_, dtExpired_));
            }
        public:
            bool    Open(const std::string& sDriver_) {
                return session_controler::open(sDriver_, SESSION_VERSION_MIN, SESSION_VERSION_CURRENT);
            }
        };
        */
    }
}
