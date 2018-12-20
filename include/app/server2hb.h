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
#include "util//utils.h"

namespace khorost {
    class server2_hb : public network::connection_context {
    protected:
        class cb_connection : public network::connection {
        protected:
            virtual size_t	DataProcessing(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
                server2_hb*		pServer = reinterpret_cast<server2_hb*>(GetController()->GetContext());
                return pServer->m_Dispatcher.DoProcessCB(*this, pBuffer_, nBufferSize_);
            }
        public:
            cb_connection(network::connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                network::connection(pThis_, ID_, fd_, sa_, socklen_)
                {
            }
        };

        class http_connection : public network::connection {
            bool		        m_bAuthenticate;
            std::string         m_SaltConnect;
            boost::uuids::uuid  m_idUser;

            network::http_text_protocol_header m_http;
        protected:
            virtual size_t	DataProcessing(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
                server2_hb*		pServer = reinterpret_cast<server2_hb*>(GetController()->GetContext());

                size_t nProcessBytes = m_http.process_data(*this, pBuffer_, nBufferSize_);
                if (nProcessBytes != 0) {
                    if (m_http.is_ready()) {
                        pServer->process_http(*this);
                        if (m_http.is_auto_close()) {
                            CloseConnection();
                        }
                        m_http.reset();
                    }
                }
                return nProcessBytes;
            }
        public:
            http_connection(network::connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                network::connection(pThis_, ID_, fd_, sa_, socklen_)
                , m_bAuthenticate(false) {

            }

            bool IsAuthenticate() const { return m_bAuthenticate; }
            void SetAuthenticate(bool bValue_) { m_bAuthenticate = bValue_; }

            const std::string& GetSalt(bool Reset_ = false);

            network::http_text_protocol_header&   get_http() { return m_http; }
            virtual void    GetClientIP(char* pBuffer_, size_t nBufferSize_);
        };

        db::postgres                    m_dbConnect;
        Config                          m_Configure;
        network::geo_ip_database          m_dbGeoIP;

        typedef std::map<std::string, network::token_ptr>    dict_tokens;
        dict_tokens m_refresh_tokens;
        dict_tokens m_access_tokens;
    private:
        class cb_controller : public network::connection_controller {
        public:
            virtual network::connection* CreateConnection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) {
                return new cb_connection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };
        class http_controller : public network::connection_controller{
        public:
            http_controller(connection_context* context) :
                network::connection_controller(context) {
            }

            virtual network::connection* CreateConnection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) {
                return new http_connection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };

        network::cbDispatchT<server2_hb, cb_connection, network::s2bPacket::S2B_NP_SIGNATURE>    m_Dispatcher;
        http_controller      m_Connections;

        int			            m_nHTTPListenPort;
        std::string             m_doc_root;       // место хранения вебсервера
        std::string             m_storage_root;     // альтернативное место сервера

        db::S2HBStorage         m_dbBase;
        network::session_controller   m_Sessions;

        void    TimerSessionUpdate();
        static void     stubTimerRun(server2_hb* pThis_);

        bool                                m_bShutdownTimer;
        boost::shared_ptr<boost::thread>    m_TimerThread;
        // ****************************************************************
        typedef bool (server2_hb::*funcActionS2H)(const std::string& uri_params, http_connection& connection, network::s2h_session* session);
        typedef std::map<std::string, funcActionS2H>		DictionaryActionS2H;

        DictionaryActionS2H    m_dictActionS2H;

        typedef std::function<network::session_ptr(const std::string& , boost::posix_time::ptime , boost::posix_time::ptime )> func_creator;
        virtual func_creator get_session_creator();
    protected:
        virtual bool process_http_action(const std::string& action, const std::string& uri_params, http_connection& connection);
        bool    process_http(http_connection& connection);

        virtual bool    process_http_file_server(const std::string& query_uri, http_connection& connection);
        
        virtual const char* GetContextDefaultName() const {
            return "s2h"; 
        }
        virtual const char* get_session_code() const {
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
        virtual const char* get_url_prefix_storage() const {
            return "/storage/";
        }

        network::session_ptr processing_session(http_connection& connection, network::http_text_protocol_header& http);

        network::token_ptr parse_token(khorost::network::http_text_protocol_header& http, bool is_access_token,
                                       const boost::posix_time::ptime& check);
        static void fill_json_token(const network::token_ptr& token, Json::Value& value);

        bool action_refresh_token(const std::string& params_uri, http_connection& connection, khorost::network::s2h_session* session);
        bool action_auth(const std::string& uri_params, http_connection& connection, network::s2h_session* session);

        network::token_ptr find_refresh_token(const std::string& refresh_token);
        network::token_ptr find_access_token(const std::string& access_token);
        void update_tokens(const network::token_ptr& token, const std::string& access_token,
                           const std::string& refresh_token);
        void append_token(const khorost::network::token_ptr& token);

        void remove_token(const khorost::network::token_ptr& token) {
            remove_token(token->get_access_token(), token->get_refresh_token());
        }

        void remove_token(const std::string& access_token, const std::string& refresh_token);

        std::string     m_sConfigFileName;
#if defined(_WIN32) || defined(_WIN64)
        bool                    m_bRunAsService;
        std::string             m_sServiceName;
        std::string             m_sServiceDisplayName;
        SERVICE_STATUS          m_ss;
        SERVICE_STATUS_HANDLE   m_ssHandle;

        bool    CallServerConfigurator() const { return true; }

        bool    ServiceInstall();
        bool    ServiceUninstall();

        bool    ServiceStart();
        bool    ServiceStop();
    public:
        void        ServiceReportEvent(LPTSTR szFunction);
        void        ReportServiceStatus(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_);

        void        SetSSCurrentState(DWORD dwCurrentState_) { m_ss.dwCurrentState = dwCurrentState_; }
        DWORD       GetSSCurrentState() const { return m_ss.dwCurrentState; }

        void        SetSSHandle(SERVICE_STATUS_HANDLE ssHandle_) { 
            m_ssHandle = ssHandle_; 
            m_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            m_ss.dwServiceSpecificExitCode = 0;
        }
        const char* GetServiceName() const { return m_sServiceName.c_str(); }
#endif
    public:
        server2_hb ();
        virtual ~server2_hb () = default;

        virtual bool    Shutdown();
        virtual bool    CheckParams(int argc_, char* argv_[], int& nResult_, g3::LogWorker* logger_ = NULL);
        virtual bool    PrepareToStart();
        virtual bool    AutoExecute();
        virtual bool    Startup();
        virtual bool    Run();
        virtual bool    Finish();

        static void parse_action(const std::string& query, std::string& action, std::string& params);
        static std::string json_string(const Json::Value& value, bool styled = false);
        static void json_fill_auth(network::s2h_session* session, bool full_info, Json::Value& value);

        void    SetConnect(std::string sHost_, int nPort_, std::string sDatabase_, std::string sLogin_, std::string sPassword_) {
            m_dbConnect.SetConnect(sHost_, nPort_, sDatabase_, sLogin_, sPassword_);
        }
        void	SetListenPort(int nPort_) { m_nHTTPListenPort = nPort_; }
        //        void    SetStorageFolder(const std::string& strFolder_);
        void    SetHTTPDocRoot(const std::string& sDocRoot_, const std::string& sStorageRoot_) {
            m_doc_root = sDocRoot_;
            m_storage_root = sStorageRoot_;
        }
        void    SetSessionDriver(const std::string& driver);

    private:

    };
    extern server2_hb* g_pS2HB;
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

#define S2H_JSON_AUTH                       "auth"
#define S2H_JSON_NICKNAME                   "nickname"
#define S2H_JSON_SALT                       "salt"
#define S2H_JSON_REASON                     "reason"
#define S2H_JSON_ROLES                      "roles"

#define S2H_DEFAULT_TCP_PORT                7709

#define KHL_HTTP_PARAM__AUTHORIZATION   "Authorization"
#define KHL_JSON_PARAM__DURATION "0dur"

#define KHL_SET_TIMESTAMP_MILLISECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::EpochDiff(json_value).total_milliseconds()
#define KHL_SET_TIMESTAMP_MICROSECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::EpochDiff(json_value).total_microseconds()

#define KHL_SET_CPU_DURATION(json_object, json_tag, now_value) \
    json_object[json_tag] = (boost::posix_time::microsec_clock::universal_time() - now_value).total_microseconds()

#endif // __SERVER_2HB__
