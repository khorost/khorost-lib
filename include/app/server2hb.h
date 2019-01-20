#pragma once

#include <boost/uuid/uuid.hpp>

#include "net/http.h"
#include "app/khl-define.h"
#include "app/framework.h"
#include "app/server2hb-np.h"
#include "app/s2h-session.h"
#include "app/s2hb-storage.h"
#include "app/config.h"
#include "util/utils.h"

namespace khorost {
    class server2_hb : public network::connection_context {
    protected:
        class cb_connection : public network::connection {
        protected:
            virtual size_t	data_processing(const boost::uint8_t* buffer, const size_t buffer_size) {
                auto server = reinterpret_cast<server2_hb*>(get_controller()->get_context());
                return server->m_dispatcher.DoProcessCB(*this, buffer, buffer_size);
            }
        public:
            cb_connection(network::connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                network::connection(pThis_, ID_, fd_, sa_, socklen_)
                {
            }
        };

        class http_connection final : public network::connection {
            bool		        m_authenticate;
            std::string         m_salt_connect;
            boost::uuids::uuid  m_id_user{};

            network::http_text_protocol_header_ptr m_http;
            size_t m_receive_bytes_prev_request, m_send_bytes_prev_request;
        protected:
            size_t	data_processing(const boost::uint8_t* buffer, const size_t buffer_size) override {
                auto server = reinterpret_cast<server2_hb*>(get_controller()->get_context());
                const auto process_bytes = m_http->process_data(*this, buffer, buffer_size);

                if (process_bytes != 0) {
                    if (m_http->is_ready()) {
                        server->process_http(*this);

                        const auto& request = m_http->get_request();
                        const auto& response = m_http->get_response();
                        const auto response_real_length = m_http->is_send_data() ? response.m_content_length_ : 0;
                        const auto logger = server->get_logger();
                        logger->debug("[HTTP] processed traffic in [Head={:d}+Body={:d}], out [Head={:d}+Body={:d}] with status {:d} and connection {}"
                                      , m_receive_bytes - m_receive_bytes_prev_request - request.m_content_length_
                                      , request.m_content_length_
                                      , m_send_bytes - m_send_bytes_prev_request - response_real_length
                                      , response_real_length
                                      , response.m_nCode
                                      , m_http->is_auto_close() ? HTTP_ATTRIBUTE_CONNECTION__CLOSE : HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE);

                        if (m_http->is_auto_close()) {
                            close_connection();
                        } else {
                            m_receive_bytes_prev_request = m_receive_bytes;
                            m_send_bytes_prev_request = m_send_bytes;
                        }

                        m_http->clear();
                    }
                }
                return process_bytes;
            }
        public:
            http_connection(network::connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) :
                network::connection(pThis_, ID_, fd_, sa_, socklen_)
                , m_authenticate(false)
                , m_receive_bytes_prev_request(0)
                , m_send_bytes_prev_request(0) {
                m_http = boost::make_shared<network::http_text_protocol_header>();
            }

            bool is_authenticate() const { return m_authenticate; }
            void set_authenticate(const bool value) { m_authenticate = value; }

            const std::string& get_salt(bool Reset_ = false);

            const network::http_text_protocol_header_ptr&   get_http() const { return m_http; }
            virtual void    get_client_ip(char* buffer, size_t buffer_size);
        };

        db::postgres m_db_connect_;
        config m_configure_;
        std::string m_geoip_city_path_;
        std::string m_geoip_asn_path_;
        std::shared_ptr<spdlog::logger> m_logger_;

        typedef std::map<std::string, network::token_ptr>    dict_tokens;
        dict_tokens m_refresh_tokens;
        dict_tokens m_access_tokens;
    private:
        class cb_controller final : public network::connection_controller {
        public:
            network::connection* create_connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) override {
                return new cb_connection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };
        class http_controller final : public network::connection_controller{
        public:
            http_controller(connection_context* context) :
                network::connection_controller(context) {
            }

            network::connection* create_connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_) override {
                return new http_connection(pThis_, ID_, fd_, sa_, socklen_);
            }
        };

        network::cbDispatchT<server2_hb, cb_connection, network::s2bPacket::S2B_NP_SIGNATURE>    m_dispatcher;
        http_controller      m_connections;

        int			            m_nHTTPListenPort;
        std::string             m_doc_root;       // место хранения вебсервера
        std::string             m_storage_root;     // альтернативное место сервера

        db::S2HBStorage         m_db_base;
        network::session_controller   m_sessions;

        void    timer_session_update();
        static void     stub_timer_run(server2_hb* server);

        bool                                m_shutdown_timer;
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
        
        virtual const char* get_context_default_name() const {
            return "s2h"; 
        }
        virtual const char* get_session_code() const {
            return "s2hsession";
        }
        virtual const char* get_url_prefix_action() const {
            return "/api/";
        }
        virtual const char* get_url_param_action() const {
            return "ppa";
        }
        virtual const char* get_url_param_action_param() const {
            return "ap";
        }
        virtual const char* get_url_prefix_storage() const {
            return "/storage/";
        }

        network::session_ptr processing_session(http_connection& connect);

        network::token_ptr parse_token(const khorost::network::http_text_protocol_header_ptr& http, bool is_access_token,
                                       const boost::posix_time::ptime& check);
        static void fill_json_token(const network::token_ptr& token, Json::Value& value);

        bool action_refresh_token(const std::string& params_uri, http_connection& connection, khorost::network::s2h_session* session);
        bool action_auth(const std::string& uri_params, http_connection& connection, network::s2h_session* session);

        network::token_ptr find_token(bool is_access_token, const std::string& token_id);
        void update_tokens(const network::token_ptr& token, const std::string& access_token,
                           const std::string& refresh_token);
        void append_token(const khorost::network::token_ptr& token);

        void remove_token(const khorost::network::token_ptr& token) {
            remove_token(token->get_access_token(), token->get_refresh_token());
        }

        void remove_token(const std::string& access_token, const std::string& refresh_token);

        std::string     m_sConfigFileName;

        std::shared_ptr<spdlog::logger> get_logger();
#if defined(_WIN32) || defined(_WIN64)
        bool                    m_run_as_service;
        std::string             m_service_name;
        std::string             m_service_display_name;
        SERVICE_STATUS          m_service_status;
        SERVICE_STATUS_HANDLE   m_service_status_handle;

        bool    call_server_configurator() const { return true; }

        bool    service_install();
        bool    service_uninstall();

        bool    service_start();
        bool    service_stop();
    public:
        void        service_report_event(LPTSTR szFunction);
        void        report_service_status(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_);

        void        set_ss_current_state(DWORD dwCurrentState_) { m_service_status.dwCurrentState = dwCurrentState_; }
        DWORD       get_ss_current_state() const { return m_service_status.dwCurrentState; }

        void        set_ss_handle(SERVICE_STATUS_HANDLE ssHandle_) { 
            m_service_status_handle = ssHandle_; 
            m_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            m_service_status.dwServiceSpecificExitCode = 0;
        }
        const char* get_service_name() const { return m_service_name.c_str(); }
#endif
    public:
        server2_hb ();
        virtual ~server2_hb () = default;

        virtual bool    shutdown();
        virtual bool    check_params(int argc, char* argv[], int& result);
        virtual bool    prepare_to_start();
        virtual bool    auto_execute();
        virtual bool    startup();
        virtual bool    run();
        virtual bool    finish();

        static void parse_action(const std::string& query, std::string& action, std::string& params);
        static void json_fill_auth(network::s2h_session* session, bool full_info, Json::Value& value);

        void    set_connect(std::string sHost_, int nPort_, std::string sDatabase_, std::string sLogin_, std::string sPassword_) {
            m_db_connect_.set_connect(sHost_, nPort_, sDatabase_, sLogin_, sPassword_);
        }
        void	set_listen_port(int nPort_) { m_nHTTPListenPort = nPort_; }
        //        void    SetStorageFolder(const std::string& strFolder_);
        void    set_http_doc_root(const std::string& sDocRoot_, const std::string& sStorageRoot_) {
            m_doc_root = sDocRoot_;
            m_storage_root = sStorageRoot_;
        }
        void    set_session_driver(const std::string& driver);

    private:

    };
    extern server2_hb* g_pS2HB;
}
