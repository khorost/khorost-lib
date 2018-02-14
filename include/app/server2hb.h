#ifndef __SERVER_2HB__
#define __SERVER_2HB__

#include <boost/uuid/uuid.hpp>

#include "net/http.h"
#include "app/framework.h"
#include "app/server2hb-np.h"
#include "app/s2h-session.h"
#include "app/s2hb-storage.h"
#include "app/config.h"
#include "net/geoip.h"

namespace khorost {
    class Server2HB : public Network::ConnectionContext {
    protected:
        class CBConnection : public Network::Connection {
        protected:
            virtual size_t	DataProcessing(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
                Server2HB*		pServer = reinterpret_cast<Server2HB*>(GetController()->GetContext());
                return pServer->m_Dispatcher.DoProcessCB(*this, pBuffer_, nBufferSize_);
            }
        public:
            CBConnection(Network::ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                Network::Connection(pThis_, ID_, fd_, sa_, socklen_)
                {
            }
        };
        class HTTPConnection : public Network::Connection {
            bool		        m_bAuthenticate;
            std::string         m_SaltConnect;
            boost::uuids::uuid  m_idUser;

            Network::HTTPTextProtocolHeader          m_HTTP;
        protected:
            virtual size_t	DataProcessing(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
                Server2HB*		pServer = reinterpret_cast<Server2HB*>(GetController()->GetContext());

                size_t nProcessBytes = m_HTTP.ProcessData(*this, pBuffer_, nBufferSize_);
                if (nProcessBytes != 0) {
                    if (m_HTTP.IsReady()) {
                        pServer->ProcessHTTP(*this);
                        if (m_HTTP.IsAutoClose()) {
                            CloseConnection();
                        }
                        m_HTTP.Reset();
                    }
                }
                return nProcessBytes;
            }
        public:
            HTTPConnection(Network::ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                Network::Connection(pThis_, ID_, fd_, sa_, socklen_)
                , m_bAuthenticate(false) {

            }

            bool IsAuthenticate() const { return m_bAuthenticate; }
            void SetAuthenticate(bool bValue_) { m_bAuthenticate = bValue_; }

            const std::string& GetSalt(bool Reset_ = false);

            Network::HTTPTextProtocolHeader&   GetHTTP() { return m_HTTP; }
            virtual void    GetClientIP(char* pBuffer_, size_t nBufferSize_);
        };

        DB::Postgres                    m_dbConnect;
        Config                          m_Configure;
        Network::GeoIPDatabase          m_dbGeoIP;
    private:
        class CBController : public Network::ConnectionController {
        public:
            virtual Network::Connection* CreateConnection(ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) {
                return new CBConnection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };
        class HTTPController : public Network::ConnectionController {
        public:
            HTTPController(ConnectionContext* pContext_) :
                Network::ConnectionController(pContext_) {
            }

            virtual Network::Connection* CreateConnection(ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) {
                return new HTTPConnection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };

        Network::cbDispatchT<Server2HB, CBConnection, Network::s2bPacket::S2B_NP_SIGNATURE>    m_Dispatcher;
        HTTPController      m_Connections;

        int			            m_nHTTPListenPort;
        std::string             m_strDocRoot;       // место хранения вебсервера
        std::string             m_sStorageRoot;     // альтернативное место сервера

        DB::S2HBStorage                 m_dbBase;
        Network::S2HSessionController   m_Sessions;

        void    TimerSessionUpdate();
        static void     stubTimerRun(Server2HB* pThis_);

        bool                                m_bShutdownTimer;
        boost::shared_ptr<boost::thread>    m_TimerThread;
        // ****************************************************************
        typedef bool (Server2HB::*funcActionS2H)(Network::Connection& rConnect_, Network::S2HSession* sp_, Network::HTTPTextProtocolHeader& rHTTP_);
        typedef std::map<std::string, funcActionS2H>		DictionaryActionS2H;

        DictionaryActionS2H    m_dictActionS2H;
    protected:
        bool    ProcessHTTP(HTTPConnection& rConnect_);

        virtual bool    ProcessHTTPCommand(const std::string& sQueryAction_, Network::S2HSession* sp_, HTTPConnection& rConnect_, Network::HTTPTextProtocolHeader& rHTTP_);
        virtual bool    ProcessHTTPFileServer(const std::string& sQueryURI_, Network::S2HSession* sp_, HTTPConnection& rConnect_, Network::HTTPTextProtocolHeader& rHTTP_);
        
        virtual const char* GetContextDefaultName() const {
            return "s2h"; 
        }
        virtual const char* GetSessionCode() const {
            return "s2hsession";
        }
        virtual const char* GetURLPrefixAction() const {
            return "/api/";
        }
        virtual const char* GetURLParamAction() const {
            return "ppa";
        }
        virtual const char* GetURLParamActionParam() const {
            return "ap";
        }
        virtual const char* GetURLPrefixStorage() const {
            return "/storage/";
        }

        Network::SessionPtr ProcessingSession(HTTPConnection& rConnect_, Network::HTTPTextProtocolHeader& rHTTP_);

        bool    ActionAuth(Network::Connection& rConnect_, Network::S2HSession* sp_, Network::HTTPTextProtocolHeader& rHTTP_);

        std::string     m_sConfigFileName;
#if defined(_WIN32) || defined(_WIN64)
        bool                    m_bRunAsService;
        std::string             m_sServiceName;
        std::string             m_sServiceDisplayName;
        SERVICE_STATUS          m_ss;
        SERVICE_STATUS_HANDLE   m_ssHandle;

        bool    CallServerConfigurator() { return true; }

        bool    ServiceInstall();
        bool    ServiceUninstall();

        bool    ServiceStart();
        bool    ServiceStop();
    public:
        void        ServiceReportEvent(LPTSTR szFunction);
        void        ReportServiceStatus(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_);

        void        SetSSCurrentState(DWORD dwCurrentState_) { m_ss.dwCurrentState = dwCurrentState_; }
        DWORD       GetSSCurrentState() { return m_ss.dwCurrentState; }

        void        SetSSHandle(SERVICE_STATUS_HANDLE ssHandle_) { 
            m_ssHandle = ssHandle_; 
            m_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            m_ss.dwServiceSpecificExitCode = 0;
        }
        const char* GetServiceName() { return m_sServiceName.c_str(); }
#endif
    public:
        Server2HB ();
        virtual ~Server2HB ();

        virtual bool    Shutdown();
        virtual bool    CheckParams(int argc_, char* argv_[], int& nResult_, g3::LogWorker* logger_ = NULL);
        virtual bool    PrepareToStart();
        virtual bool    AutoExecute();
        virtual bool    Startup();
        virtual bool    Run();
        virtual bool    Finish();

        std::string JSONWrite(const Json::Value& jvRoot_, bool bStyled_) const;

        void        JSON_PingPong(Network::HTTPTextProtocolHeader& rHTTP_, Json::Value& jvRoot_) const;
        bool        JSON_FillAuth(Network::S2HSession* pSession_, bool bFullInfo_, Json::Value& jvRoot_) const;

        void    SetConnect(std::string sHost_, int nPort_, std::string sDatabase_, std::string sLogin_, std::string sPassword_) {
            m_dbConnect.SetConnect(sHost_, nPort_, sDatabase_, sLogin_, sPassword_);
        }
        void	SetListenPort(int nPort_) { m_nHTTPListenPort = nPort_; }
        //        void    SetStorageFolder(const std::string& strFolder_);
        void    SetHTTPDocRoot(const std::string& sDocRoot_, const std::string& sStorageRoot_) {
            m_strDocRoot = sDocRoot_;
            m_sStorageRoot = sStorageRoot_;
        }
        void    SetSessionDriver(const std::string& sDriver_) {
            m_Sessions.Open(sDriver_);
        }

    private:

    };
    extern Server2HB* g_pS2HB;
}

#define S2H_PARAM_ACTION_AUTH               "auth"      // авторизация пользователя
#define S2H_PARAM_ACTION_AUTH_CHECK         "ca"        // проверка авторизации
#define S2H_PARAM_ACTION_AUTH_DO            "da"        // подтверждение авторизации
#define S2H_PARAM_ACTION_AUTH_PRE           "ba"        // запрос авторизации
#define S2H_PARAM_ACTION_AUTH_RESET         "ra"        // сброс авторизации
#define S2H_PARAM_ACTION_AUTH_CHANGEPASS    "chpwd"     // изменить пароль

#define S2H_JSON_PONG                       "pong"
#define S2H_PARAM_ACTION_PING               "ping"

#define S2H_PARAM_HUMMAN_JSON               "hjs"

#define S2H_PARAM_QUESTION                  "q"
#define S2H_PARAM_HASH                      "h"
#define S2H_PARAM_ADMIN_PASSWORD            "admpwd"
#define S2H_PARAM_LOGIN_PASSWORD            "loginpwd"
#define S2H_PARAM_PASSWORD                  "pwd"

#define S2H_JSON_AUTH                       "Auth"
#define S2H_JSON_NICKNAME                   "Nickname"
#define S2H_JSON_SALT                       "Salt"
#define S2H_JSON_REASON                     "Reason"
#define S2H_JSON_ROLES                      "Roles"

#define S2H_DEFAULT_TCP_PORT                7709

#endif // __SERVER_2HB__
