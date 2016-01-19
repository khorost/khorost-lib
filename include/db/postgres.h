#ifndef __POSTGRES_H__
#define __POSTGRES_H__

#ifndef WIN32
/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
#include <unistd.h>
#else
#include <winsock2.h>
#endif  // Win32

#include <string>
#include <pqxx/pqxx>
#include <boost/scoped_ptr.hpp>
#include <json/json.h>

namespace DB {
    class Postgres {
        int         m_nPort;
        std::string m_sHost;
        std::string m_sDatabase;
        std::string m_sLogin;
        std::string m_sPassword;
    protected:
        boost::scoped_ptr<pqxx::connection> m_DBConnection;
    public:
        Postgres() {
        }
        virtual ~Postgres() {
            Disconnect();
        }

        bool Reconnect();
        bool Disconnect();

        bool CheckConnect();

        void    SetConnect(std::string sHost_, int nPort_, std::string sDatabase_, std::string sLogin_, std::string sPassword_) {
            m_sHost = sHost_;
            m_nPort = nPort_;
            m_sDatabase = sDatabase_;
            m_sLogin = sLogin_;
            m_sPassword = sPassword_;

            Reconnect();
        }

        void    ExecuteCustomSQL(bool bReadOnly_, const std::string& sSQL_, Json::Value& jvResult_);

    };
}

#endif  // __POSTGRES_H__
