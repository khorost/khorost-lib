#pragma once

#include <boost/uuid/uuid.hpp>

#include "net/http.h"
#include "app/config.h"
#include "app/khl-define.h"
#include "app/framework.h"
#include "system/taskqueue.hxx"

namespace khorost {
    class server2_h : public network::connection_context {
    protected:
        class HttpTask;

        class http_connection final : public network::connection {
            network::http_text_protocol_header* m_http_processor;
            size_t m_receive_bytes_prev_request, m_send_bytes_prev_request;
        protected:
            size_t data_processing(const boost::uint8_t* buffer, const size_t buffer_size) override {
                auto server = reinterpret_cast<server2_h*>(get_controller()->get_context());
                if (m_http_processor == nullptr) {
                    m_http_processor = new network::http_text_protocol_header();
                }
                const auto process_bytes = m_http_processor->process_data(buffer, buffer_size);

                if (process_bytes != 0) {
                    if (m_http_processor->is_ready()) {
                        HttpTask task(server, this, m_http_processor) ;
                        task.processing();
//                        server->m_queue.QueueTask(HttpTask(server, this, m_http_processor));
                        m_http_processor = nullptr;
                    }
                }
                return process_bytes;
            }

        public:
            http_connection(network::connection_controller* controller, int id, evutil_socket_t fd, struct sockaddr* sa) :
                network::connection(controller, id, fd, sa)
                ,m_http_processor(nullptr)
                , m_receive_bytes_prev_request(0)
                , m_send_bytes_prev_request(0) {
            }

            virtual ~http_connection() {
                if (m_http_processor != nullptr) {
                    delete m_http_processor;
                    m_http_processor = nullptr;
                }
            }

            void get_client_ip(const khorost::network::http_text_protocol_header* http,char* buffer, size_t buffer_size);

            void update_prev_bytes() {
                m_receive_bytes_prev_request = m_receive_bytes;
                m_send_bytes_prev_request = m_send_bytes;
            }

            size_t get_last_receive_bytes() {
                return m_receive_bytes - m_receive_bytes_prev_request;
            }

            size_t get_last_send_bytes() {
                return m_send_bytes - m_send_bytes_prev_request;
            }
        };

        class http_controller final : public network::connection_controller {
        public:
            http_controller(connection_context* context) :
                network::connection_controller(context) {
            }

            network::connection* create_connection(connection_controller* controller, int id, evutil_socket_t fd,struct sockaddr* sa) override {
                return new http_connection(controller, id, fd, sa);
            }
        };

        class HttpTask {
        public:
            typedef bool result_type;

            server2_h* m_server;
            http_connection* m_connection;
            network::http_text_protocol_header* m_http_processor;

            HttpTask(server2_h* server, http_connection* connection, network::http_text_protocol_header* http_processor) :
                m_server(server),
                m_connection(connection),
                m_http_processor(http_processor){
            }

            bool operator()() {
                return processing();
            }

            bool processing() {
                auto logger = m_server->get_logger();

                m_server->process_http(*m_connection, m_http_processor);

                const auto& request = m_http_processor->get_request();
                const auto& response = m_http_processor->get_response();
                const auto response_real_length = m_http_processor->is_send_data() ? response.m_content_length : 0;
                logger->debug(
                    "[HTTP] processed traffic in [Head={:d}+Body={:d}], out [Head={:d}+Body={:d}] with status {:d} and connection {}"
                    , m_connection->get_last_receive_bytes()  - request.m_content_length, request.m_content_length
                    , m_connection->get_last_send_bytes()  - response_real_length, response_real_length
                    , response.m_code
                    , m_http_processor->is_auto_close()
                    ? HTTP_ATTRIBUTE_CONNECTION__CLOSE
                    : HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE);

                if (m_http_processor->is_auto_close()) {
                    m_connection->close_connection();
                } else {
                    m_connection->update_prev_bytes();
                }

                delete m_http_processor;

                return true;
            }

        };

        TaskQueue<HttpTask> m_queue;
        http_controller m_connections;
        config m_configure_;

        unsigned short m_nHTTPListenPort;
        std::string m_doc_root; // место хранения вебсервера
        std::string m_storage_root; // альтернативное место сервера
        std::shared_ptr<spdlog::logger> m_logger_ = nullptr;
        std::shared_ptr<spdlog::logger> m_logger_profiler_ = nullptr;

        std::string m_sConfigFileName;

        virtual const char* get_url_prefix_action() const {
            return "/api/";
        }

        virtual const char* get_context_default_name() const {
            return "s2h";
        }

        typedef bool (server2_h::*funcActionS2Hs)(const std::string& uri_params, http_connection& connection, khorost::network::http_text_protocol_header* http);
        typedef std::map<std::string, funcActionS2Hs> DictionaryActionS2Hs;

        DictionaryActionS2Hs m_dictActionS2Hs;

        void set_listen_port(int nPort_) { m_nHTTPListenPort = nPort_; }
        void set_http_doc_root(const std::string& sDocRoot_, const std::string& sStorageRoot_) {
            m_doc_root = sDocRoot_;
            m_storage_root = sStorageRoot_;
        }

#if defined(_WIN32) || defined(_WIN64)
        bool m_run_as_service;
        std::string m_service_name;
        std::string m_service_display_name;
        SERVICE_STATUS m_service_status;
        SERVICE_STATUS_HANDLE m_service_status_handle;

        bool call_server_configurator() const { return true; }

        bool service_install();
        bool service_uninstall();

        bool service_start();
        bool service_stop();
    public:
        void service_report_event(LPTSTR szFunction);
        void report_service_status(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_);

        void set_ss_current_state(DWORD dwCurrentState_) { m_service_status.dwCurrentState = dwCurrentState_; }
        DWORD get_ss_current_state() const { return m_service_status.dwCurrentState; }

        void set_ss_handle(SERVICE_STATUS_HANDLE ssHandle_) {
            m_service_status_handle = ssHandle_;
            m_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            m_service_status.dwServiceSpecificExitCode = 0;
        }

        const char* get_service_name() const { return m_service_name.c_str(); }
#endif

    public:
        server2_h();
        virtual ~server2_h() = default;

        virtual bool check_params(int argc, char* argv[], int& result);
        virtual bool prepare_to_start();
        virtual bool auto_execute();
        virtual bool startup();
        virtual bool run();
        virtual bool finish();
        virtual bool shutdown();

        virtual bool process_http(http_connection& connection, khorost::network::http_text_protocol_header* http);
        virtual bool process_http_file_server(const std::string& query_uri, http_connection& connection, khorost::network::http_text_protocol_header* http);
        virtual bool process_http_action(const std::string& action, const std::string& uri_params, http_connection& connection, khorost::network::http_text_protocol_header* http);

        std::shared_ptr<spdlog::logger> get_logger();
        std::shared_ptr<spdlog::logger> get_logger_profiler();

        static void parse_action(const std::string& query, std::string& action, std::string& params);
    };
    extern server2_h* g_pS2HB;

}