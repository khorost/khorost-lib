#pragma once

#ifndef _NO_USE_REDIS
#include <cpp_redis/cpp_redis>
#endif // _NO_USE_REDIS

#include "net/http.h"
#include "app/khl-define.h"
#include "app/framework.h"
#include "app/server2hb-np.h"
#include "app/s2h-session.h"
#include "app/s2hb-storage.h"
#include "util/utils.h"
#include "util/i18n.h"
#include "util/profiler.h"
#include "server2h.h"

namespace khorost {
    class server2_hb : public server2_h {
    protected:
        class cb_connection : public network::connection {
        protected:
            virtual size_t data_processing(const boost::uint8_t* buffer, const size_t buffer_size) {
                auto server = reinterpret_cast<server2_hb*>(get_controller()->get_context());
                return server->m_dispatcher.DoProcessCB(*this, buffer, buffer_size);
            }

        public:
            cb_connection(network::connection_controller* controller, int id, evutil_socket_t fd, struct sockaddr* sa) :
                network::connection(controller, id, fd, sa) {
            }
        };

        db::postgres m_db_connect_;
        std::string m_geoip_city_path_;
        std::string m_geoip_asn_path_;

        typedef std::map<std::string, network::token_ptr> dict_tokens;
        dict_tokens m_tokens_;
#ifndef _NO_USE_REDIS
        cpp_redis::client m_cache_db_;
#endif // _NO_USE_REDIS
        std::string m_cache_db_context_;

        virtual const char* get_cache_set_tag() const { return "id"; }
    private:
        class cb_controller final : public network::connection_controller {
        public:
            network::connection*
            create_connection(connection_controller* controller, int id, evutil_socket_t fd, struct sockaddr* sa) override {
                return new cb_connection(controller, id, fd, sa);
            }
        };

        network::cbDispatchT<server2_hb, cb_connection, network::s2bPacket::S2B_NP_SIGNATURE> m_dispatcher;

        db::S2HBStorage m_db_base;
        network::session_controller m_sessions;

        void timer_session_update();
        static void stub_timer_run(server2_hb* server);

        bool m_shutdown_timer;
        std::shared_ptr<std::thread> m_TimerThread;
        // ****************************************************************
        typedef bool (server2_hb::*funcActionS2H)(const std::string& uri_params, http_connection& connection,
                                                  const khorost::network::http_text_protocol_header_ptr& http,
                                                  network::s2h_session* session);
        typedef std::map<std::string, funcActionS2H> DictionaryActionS2H;

        DictionaryActionS2H m_dictActionS2H;

        typedef std::function<network::session_ptr(const std::string&, boost::posix_time::ptime,
                                                   boost::posix_time::ptime)> func_creator;
        virtual func_creator get_session_creator();
    protected:
        bool process_http(http_connection& connection, const khorost::network::http_text_protocol_header_ptr& http) override;
        bool process_http_file_server(const std::string& query_uri, http_connection& connection,
                                      const khorost::network::http_text_protocol_header_ptr& http) override;
        bool process_http_action(const std::string& action, const std::string& uri_params,
                                 http_connection& connection, const khorost::network::http_text_protocol_header_ptr& http) override;

        virtual const char* get_session_code() const {
            return "s2hsession";
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

        network::session_ptr processing_session(http_connection& connect, const khorost::network::http_text_protocol_header_ptr& http);

        network::token_ptr parse_token(const khorost::network::http_text_protocol_header_ptr& http,
                                       bool is_access_token,
                                       const boost::posix_time::ptime& check);
        static void fill_json_token(const network::token_ptr& token, Json::Value& value);

        bool action_refresh_token(const std::string& params_uri, http_connection& connection,
                                  const khorost::network::http_text_protocol_header_ptr& http,
                                  khorost::network::s2h_session* session);
        bool action_auth(const std::string& uri_params, http_connection& connection,
                         const khorost::network::http_text_protocol_header_ptr& http, network::s2h_session* session);

        network::token_ptr find_token(bool is_access_token, const std::string& token_id);
        void update_tokens(const network::token_ptr& token, const std::string& access_token,
                           const std::string& refresh_token);
        void append_token(const khorost::network::token_ptr& token);

        void remove_token(const khorost::network::token_ptr& token) {
            remove_token(token->get_access_token(), token->get_refresh_token());
        }

        void remove_token(const std::string& access_token, const std::string& refresh_token);
        void remove_token(const std::string& token_id);

    public:
        server2_hb();
        virtual ~server2_hb() = default;

        bool shutdown() override;
        bool prepare_to_start() override;
        bool auto_execute() override;
        bool startup() override;
        bool run() override;

        static void json_fill_auth(network::s2h_session* session, bool full_info, Json::Value& value);

        void set_connect(const std::string& host, const int port, const std::string& database, const std::string& login,
                         const std::string& password, int pool_size) {
            PROFILER_FUNCTION_TAG(get_logger_profiler()
                                  , fmt::format("[host={} port={} user={} password=*** database={} pool_size={}]"
                                      , host, port, login, database, pool_size));
            m_db_connect_.set_connect(host, port, database, login, password, pool_size);
        }

        void set_session_driver(const std::string& driver);
        //        void    SetStorageFolder(const std::string& strFolder_);

    private:

    };

}
