// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "app/server2hb.h"
#include "net/geoip.h"

#include <openssl/md5.h>
#include <boost/algorithm/hex.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

namespace po = boost::program_options;
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace khorost;

void RecalcPasswordHash(std::string& sPwHash_, const std::string& sLogin_, const std::string& sPassword_, const std::string& sSalt_) {
    std::vector<unsigned char> md(MD5_DIGEST_LENGTH);
    MD5_CTX ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sLogin_.c_str(), sLogin_.size());
    MD5_Update(&ctx, sPassword_.c_str(), sPassword_.size());
    MD5_Update(&ctx, sSalt_.c_str(), sSalt_.size());
    MD5_Final(md.data(), &ctx);

    sPwHash_.erase();
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwHash_));
}

bool IsPasswordHashEqual2(const std::string& sChecked_, const std::string& sFirst_, const std::string& sSecond_) {
    std::vector<unsigned char> md(MD5_DIGEST_LENGTH);
    MD5_CTX ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sFirst_.c_str(), sFirst_.size());
    MD5_Update(&ctx, sSecond_.c_str(), sSecond_.size());
    MD5_Final(md.data(), &ctx);

    std::string sPwhashSession;
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwhashSession));

    return sChecked_ == sPwhashSession;
}

bool IsPasswordHashEqual3(const std::string& sChecked_, const std::string& sFirst_, const std::string& sSecond_,
                          const std::string& sThird_) {
    std::vector<unsigned char> md(MD5_DIGEST_LENGTH);
    MD5_CTX ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sFirst_.c_str(), sFirst_.size());
    MD5_Update(&ctx, sSecond_.c_str(), sSecond_.size());
    MD5_Update(&ctx, sThird_.c_str(), sThird_.size());
    MD5_Final(md.data(), &ctx);

    std::string sPwhashSession;
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwhashSession));

    return sChecked_ == sPwhashSession;
}

server2_hb::server2_hb() :
    server2_h()
    , m_db_base(m_db_connect_)
    , m_dispatcher(this)
    , m_shutdown_timer_(false) {
}

bool server2_hb::shutdown() {
    m_shutdown_timer_ = true;
    return server2_h::shutdown();
}

bool server2_hb::prepare_to_start() {
    server2_h::prepare_to_start();

    auto logger = get_logger();

    set_session_driver(m_configure_.get_value("http:session", "./session.db"));

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &server2_hb::action_auth));
    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>("refresh_token", &server2_hb::action_refresh_token));

    set_connect(
        m_configure_.get_value("storage:host", "localhost")
        , m_configure_.get_value("storage:port", 5432)
        , m_configure_.get_value("storage:db", "")
        , m_configure_.get_value("storage:user", "")
        , m_configure_.get_value("storage:password", "")
        , m_configure_.get_value("storage:pool", 7)
    );

    logger->debug("[GEOIP] MMDB version: {}", khorost::network::geo_ip_database::get_lib_version_db());

    khorost::network::geo_ip_database db;

    const auto geoip_city_path = m_configure_.get_value("geoip:city", "");
    if (db.open_database(geoip_city_path)) {
        //        logger->debug("[GEOIP] meta city info \"{}\"", db.get_metadata());
        m_geoip_city_path_ = geoip_city_path;
    } else {
        logger->warn("[GEOIP] Error init MMDB City by path {}", geoip_city_path);
    }
    db.close_database();

    const auto geoip_asn_path = m_configure_.get_value("geoip:asn", "");
    if (db.open_database(geoip_asn_path)) {
        //        logger->debug("[GEOIP] meta asn info \"{}\"", db.get_metadata());
        m_geoip_asn_path_ = geoip_asn_path;
    } else {
        logger->warn("[GEOIP] Error init MMDB ASN by path {}", geoip_asn_path);
    }

    return true;
}

bool server2_hb::auto_execute() {
    server2_h::auto_execute();

    auto cfg_create_user = m_configure_["autoexec"]["Create"]["User"];
    if (!cfg_create_user.isNull()) {
        for (auto cfg_user : cfg_create_user) {
            if (!m_db_base.IsUserExist(cfg_user["Login"].asString())) {
                m_db_base.CreateUser(cfg_user);
            }
        }
    }

    return true;
}

bool server2_hb::startup() {
    m_TimerThread.reset(new std::thread(boost::bind(&stub_timer_run, this)));
    return server2_h::startup();
}

bool server2_hb::run() {
    m_TimerThread->join();
    return server2_h::run();
}

bool server2_hb::process_http_action(const std::string& action, const std::string& uri_params, http_connection& connection,
                                     khorost::network::http_text_protocol_header* http) {
    const auto s2_h_session = reinterpret_cast<network::s2h_session*>(processing_session(connection, http).get());

    const auto it = m_dictActionS2H.find(action);
    if (it != m_dictActionS2H.end()) {
        const auto func_action = it->second;
        return (this->*func_action)(uri_params, connection, http, s2_h_session);
    }
    return false;
}

bool server2_hb::process_http(http_connection& connection, khorost::network::http_text_protocol_header* http) {
    auto logger = get_logger();

    const auto query_uri = http->get_query_uri();
    const auto url_prefix_action = get_url_prefix_action();
    const auto size_upa = strlen(url_prefix_action);

    logger->debug("[HTTP_PROCESS] URI='{}'", query_uri);

    if (strncmp(query_uri, url_prefix_action, size_upa) == 0) {
        std::string action, params;
        parse_action(query_uri + size_upa, action, params);
        if (process_http_action(action, params, connection, http)) {
            return true;
        }

        logger->debug("[HTTP_PROCESS] worker pQueryAction not found");

        http->set_response_status(http_response_status_not_found, "Not found");
        http->send_response(connection, "File not found");

        return false;
    }
    return process_http_file_server(query_uri, connection, http);
}

bool server2_hb::process_http_file_server(const std::string& query_uri, http_connection& connection,
                                          khorost::network::http_text_protocol_header* http) {
    const std::string prefix = get_url_prefix_storage();

    if (prefix == query_uri.substr(0, prefix.size())) {
        return http->send_file(query_uri.substr(prefix.size() - 1), connection, m_storage_root);
    }

    return server2_h::process_http_file_server(query_uri, connection, http);
}

void server2_hb::timer_session_update() {
    m_sessions.check_alive_sessions();
}

void server2_hb::stub_timer_run(server2_hb* server) {
    using namespace boost;
    using namespace posix_time;

    auto logger = server->get_logger();

    ptime session_ip_update;

    auto session_update = session_ip_update = second_clock::universal_time();

    while (!server->m_shutdown_timer_) {
        const auto now = second_clock::universal_time();

        if ((now - session_update).minutes() >= 10) {
            logger->debug("[TIMER] 10 minutes check");
            session_update = now;
            server->timer_session_update();
        }
        if ((now - session_ip_update).hours() >= 1) {
            logger->debug("[TIMER] Hours check");
            session_ip_update = now;
        }

        this_thread::sleep_for(chrono::milliseconds(1000));
    }
    // сбросить кэш
    server->timer_session_update();
}

server2_hb::func_creator server2_hb::get_session_creator() {
    return [](const std::string& session_id, boost::posix_time::ptime created,
              boost::posix_time::ptime expired) {
        return std::make_shared<network::s2h_session>(
            session_id, created, expired);
    };
}

void server2_hb::set_session_driver(const std::string& driver) {
    m_sessions.open(driver, SESSION_VERSION_MIN, SESSION_VERSION_CURRENT, get_session_creator());
}

network::session_ptr server2_hb::processing_session(http_connection& connect, khorost::network::http_text_protocol_header* http) {
    using namespace boost::posix_time;

    auto logger = get_logger();

    auto created = false;
    const auto session_id = http->get_cookie(get_session_code(), nullptr);
    auto sp = m_sessions.get_session(session_id != nullptr ? session_id : "", created, get_session_creator());
    auto* s2_h_session = reinterpret_cast<network::s2h_session*>(sp.get());

    char s_ip[255];
    connect.get_client_ip(http, s_ip, sizeof(s_ip));

    if (s2_h_session != nullptr) {
        s2_h_session->set_last_activity(second_clock::universal_time());
        s2_h_session->set_ip(s_ip);
    }

    http->set_cookie(get_session_code(), sp->get_session_id(), sp->get_expired(), http->get_host(), true);

    logger->debug("[OAUTH] {} = '{}' ClientIP = '{}' ConnectID = #{:d} InS = '{}' "
                  , get_session_code(), sp->get_session_id().c_str(), s_ip, connect.get_id()
                  , session_id != nullptr ? (strcmp(sp->get_session_id().c_str(), session_id) == 0 ? "+" : session_id) : "-"
    );
    return sp;
}

network::token_ptr server2_hb::parse_token(khorost::network::http_text_protocol_header* http, const bool is_access_token,
                                           const boost::posix_time::ptime& check) {
    PROFILER_FUNCTION_TAG(get_logger_profiler(), fmt::format("[AT={}]", is_access_token));

    static const auto token_mask = khl_token_type + std::string(" ");
    const auto logger = get_logger();
    std::string id;

    const auto header_authorization = http->get_header_parameter(khl_http_param_authorization, nullptr);
    if (header_authorization != nullptr) {
        const auto token_id = data::escape_string(header_authorization);
        const auto token_pos = token_mask.size();

        if (token_id.size() <= token_pos || token_id.substr(0, token_pos) != token_mask) {
            logger->warn("[OAUTH] Bad token format '{}'", token_id);
            return nullptr;
        }

        id = token_id.substr(token_pos);
    } else {
        id = data::escape_string(http->get_parameter("token", ""));
    }

    auto token = find_token(is_access_token, id);
    if (token != nullptr) {
        if (check != boost::date_time::neg_infin && (is_access_token && !token->is_no_expire_access(check) || !is_access_token && !token->
            is_no_expire_refresh(check))) {
            logger->debug("[OAUTH] {} Token {} expire. timestamp = {}"
                          , is_access_token ? "Access" : "Refresh"
                          , id
                          , to_iso_extended_string(is_access_token ? token->get_access_expire() : token->get_refresh_expire()));
            remove_token(token);
            return nullptr;
        }

        logger->debug("[OAUTH] {} Token {} expired after {}"
                      , is_access_token ? "Access" : "Refresh"
                      , id
                      , to_iso_extended_string(is_access_token ? token->get_access_expire() : token->get_refresh_expire()));
    } else {
        logger->warn("[OAUTH] {} Token with id = '{}' not found", is_access_token ? "Access" : "Refresh", id);
    }

    return token;
}

void server2_hb::fill_json_token(const network::token_ptr& token, Json::Value& value) {
    value["token_type"] = khl_token_type;

    value[khl_json_param_access_token] = token->get_access_token();
    value[khl_json_param_refresh_token] = token->get_refresh_token();

    value["access_expires_in"] = token->get_access_duration();
    value["refresh_expires_in"] = token->get_refresh_duration();
}

bool server2_hb::action_refresh_token(const std::string& params_uri, http_connection& connection,
                                      khorost::network::http_text_protocol_header* http, khorost::network::s2h_session* session) {
    const auto& logger = get_logger();
    Json::Value json_root;
    const auto now = boost::posix_time::microsec_clock::universal_time();

    try {
        auto token = parse_token(http, false, now);
        if (token != nullptr) {
            const auto time_refresh = http->get_parameter("time_refresh", token->get_refresh_duration());
            const auto time_access = http->get_parameter("time_access", token->get_access_duration());

            const auto prev_access_token = token->get_access_token();
            const auto prev_refresh_token = token->get_refresh_token();

            auto access_token = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
            auto refresh_token = boost::lexical_cast<std::string>(boost::uuids::random_generator()());

            data::compact_uuid_to_string(access_token);
            data::compact_uuid_to_string(refresh_token);

            const auto access_expire = now + boost::posix_time::seconds(time_access + khl_token_append_time);
            const auto refresh_expire = now + boost::posix_time::seconds(time_refresh + khl_token_append_time);

            token->set_access_token(access_token);
            token->set_access_duration(time_access);
            token->set_access_expire(access_expire);

            token->set_refresh_token(refresh_token);
            token->set_refresh_duration(time_refresh);
            token->set_refresh_expire(refresh_expire);

            const auto& payload = token->get_payload();
            const auto token_value = khorost::data::json_string(payload);
            const auto cache_set_value = payload[get_cache_set_tag()].asString();
            // clear previous state
            m_cache_db_.del({prev_access_token, prev_refresh_token});
            m_cache_db_.srem(m_cache_db_context_ + "tt:" + cache_set_value, {prev_refresh_token});
            // set new state
            m_cache_db_.setex(m_cache_db_context_ + "at:" + access_token, time_access + khl_token_append_time,
                              token_value);
            m_cache_db_.setex(m_cache_db_context_ + "rt:" + refresh_token, time_refresh + khl_token_append_time,
                              token_value);
            m_cache_db_.sadd(m_cache_db_context_ + "tt:" + cache_set_value, {refresh_token});
            m_cache_db_.sync_commit();

            logger->debug("[OAUTH] Remove token Access='{}', Refresh='{}' and append token Access='{}'@{}, Refresh='{}'@{}"
                          , prev_access_token
                          , prev_refresh_token
                          , token->get_access_token()
                          , to_iso_extended_string(token->get_access_expire())
                          , token->get_refresh_token()
                          , to_iso_extended_string(token->get_refresh_expire())
            );
            update_tokens(token, prev_access_token, prev_refresh_token);

            fill_json_token(token, json_root);
        }
    } catch (const std::exception&) {
        http->set_response_status(http_response_status_internal_server_error, "UNKNOWN_ERROR");
    }

    if (!json_root.isNull()) {
        KHL_SET_CPU_DURATION(json_root, khl_json_param_duration, now);

        http->set_content_type(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JSON);
        http->send_response(connection, data::json_string(json_root));
    } else {
        http->set_response_status(http_response_status_unauthorized, "Unauthorized");
        http->end_of_response(connection);
    }

    return true;
}

bool server2_hb::action_auth(const std::string& uri_params, http_connection& connection,
                             khorost::network::http_text_protocol_header* http, network::s2h_session* session) {
    using namespace boost::posix_time;

    Json::Value root;
    std::string action, params;
    std::string nickname, hash, salt;
    int user_id;

    parse_action(uri_params, action, params);

    if (action == "login") {
        Json::Value auth;
        const auto body = reinterpret_cast<const char*>(http->get_body());

        if (data::parse_json(body, body + http->get_body_length(), auth)) {
            const auto login = auth["login"].asString();
            const auto password = auth["password"].asString();

            if (m_db_base.get_user_info(login, user_id, nickname, hash, salt)) {
                decltype(hash) calculate_passowrd_hash;

                RecalcPasswordHash(calculate_passowrd_hash, login, password, salt);
                if (calculate_passowrd_hash == hash) {
                    session->SetUserID(user_id);
                    session->SetNickname(nickname);
                    // TODO: pSession->SetPostion(/*------* /);
                    session->SetAuthenticate(true);

                    m_db_base.GetUserRoles(user_id, session);

                    m_sessions.update_session(session);
                }
            }
        }
        json_fill_auth(session, true, root);
    } else if (action == "logout") {
        session->reset();
        session->set_expired(second_clock::universal_time());

        http->set_cookie(get_session_code(), session->get_session_id(), session->get_expired(), http->get_host(), true);

        m_sessions.remove_session(session);
    } else if (action == "change") {
        std::string login;
        const auto current_password = http->get_parameter("curpwd", nullptr);

        if (current_password != nullptr
            && m_db_base.GetUserInfo(session->GetUserID(), login, nickname, hash, salt)
            && IsPasswordHashEqual3(hash, login, current_password, salt)) {

            const auto new_password = http->get_parameter("newpwd", nullptr);
            if (http->is_parameter_exist("loginpwd")) {
                login = http->get_parameter("loginpwd", nullptr);

                if (m_db_base.get_user_info(login, user_id, nickname, hash, salt)) {
                    RecalcPasswordHash(hash, login, new_password, salt);
                    m_db_base.UpdatePassword(user_id, hash);
                } else {
                    // пользователь не найден
                }
            } else {
                RecalcPasswordHash(hash, login, new_password, salt);
                m_db_base.UpdatePassword(session->GetUserID(), hash);
            }
        } else {
            root[S2H_JSON_REASON] = "UserNotFound";
        }
        json_fill_auth(session, true, root);
    }

    http->set_content_type(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS);
    http->send_response(connection, data::json_string(root));

    return true;
}

void server2_hb::json_fill_auth(network::s2h_session* session, bool full_info, Json::Value& value) {
    value[S2H_JSON_AUTH] = session->IsAuthenticate();
    if (full_info && session->IsAuthenticate()) {
        value[S2H_JSON_NICKNAME] = session->GetNickname();
        Json::Value jvRoles;
        session->fill_roles(jvRoles);
        value[S2H_JSON_ROLES] = jvRoles;
    }
}

void server2_hb::append_token(const network::token_ptr& token) {
    if (token != nullptr) {
        m_tokens_.insert(std::make_pair(token->get_refresh_token(), token));
        m_tokens_.insert(std::make_pair(token->get_access_token(), token));
    }
}

void server2_hb::remove_token(const std::string& token_id) {
    const auto it = m_tokens_.find(token_id);
    if (it != m_tokens_.end()) {
        const auto t = it->second;
        remove_token(t->get_access_token(), t->get_refresh_token());
    }
}

void server2_hb::remove_token(const std::string& access_token, const std::string& refresh_token) {
    m_tokens_.erase(refresh_token);
    m_tokens_.erase(access_token);
}

void server2_hb::update_tokens(const network::token_ptr& token, const std::string& access_token,
                               const std::string& refresh_token) {
    remove_token(access_token, refresh_token);
    append_token(token);
}

network::token_ptr server2_hb::find_token(const bool is_access_token, const std::string& token_id) {
    if (token_id.empty()) {
        return nullptr;
    }

    const auto logger = get_logger();
    logger->debug("[find_token] 1 a={} {}", is_access_token, token_id);

    const auto it = m_tokens_.find(token_id);
    if (it != m_tokens_.end()) {
        return it->second;
    }

    logger->debug("[find_token] 2 a={} {}", is_access_token, token_id);

    const auto token_context = m_cache_db_context_ + (is_access_token ? "at:" : "rt:") + token_id;
    auto rit = m_cache_db_.exists({token_context});
    m_cache_db_.sync_commit();

    const auto exist = rit.get();
    if (!exist.is_null() && exist.as_integer() == 1) {
        auto riv = m_cache_db_.get(token_context);
        m_cache_db_.sync_commit();

        const auto cp = riv.get();
        if (!cp.is_null()) {
            logger->debug("[find_token] p a={} {} '{}'", is_access_token, token_id, cp.as_string());

            Json::Value payload;
            data::parse_json_string(cp.as_string(), payload);

            auto token = std::make_shared<network::token>(
                payload[khl_json_param_access_token].asString()
                , boost::posix_time::from_iso_extended_string(payload[khl_json_param_access_expire].asString())
                , payload[khl_json_param_refresh_token].asString()
                , boost::posix_time::from_iso_extended_string(payload[khl_json_param_refresh_expire].asString())
                , payload);

            logger->debug("[find_token] e a={} {}", is_access_token, token_id);
            append_token(token);
            return token;
        }
    } else {
        logger->debug("[find_token] r a={} {}", is_access_token, token_id);

        remove_token(token_id);
    }

    return nullptr;
}
