#if defined(_WIN32) || defined(_WIN64)
 #include <windows.h>
 #include <shlwapi.h>
 #include <strsafe.h>
#endif

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

khorost::server2_hb* khorost::g_pS2HB = nullptr;

using namespace khorost;

void RecalcPasswordHash(std::string& sPwHash_, const std::string& sLogin_, const std::string& sPassword_, const std::string& sSalt_) {
    std::vector<unsigned char>   md(MD5_DIGEST_LENGTH);
    MD5_CTX         ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sLogin_.c_str(), sLogin_.size());
    MD5_Update(&ctx, sPassword_.c_str(), sPassword_.size());
    MD5_Update(&ctx, sSalt_.c_str(), sSalt_.size());
    MD5_Final(md.data(), &ctx);

    sPwHash_.erase();
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwHash_));
}

bool IsPasswordHashEqual2(const std::string& sChecked_, const std::string& sFirst_, const std::string& sSecond_) {
    std::vector<unsigned char>   md(MD5_DIGEST_LENGTH);
    MD5_CTX         ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sFirst_.c_str(), sFirst_.size());
    MD5_Update(&ctx, sSecond_.c_str(), sSecond_.size());
    MD5_Final(md.data(), &ctx);

    std::string sPwhashSession;
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwhashSession));

    return sChecked_ == sPwhashSession;
}

bool IsPasswordHashEqual3(const std::string& sChecked_, const std::string& sFirst_, const std::string& sSecond_, const std::string& sThird_) {
    std::vector<unsigned char>   md(MD5_DIGEST_LENGTH);
    MD5_CTX         ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, sFirst_.c_str(), sFirst_.size());
    MD5_Update(&ctx, sSecond_.c_str(), sSecond_.size());
    MD5_Update(&ctx, sThird_.c_str(), sThird_.size());
    MD5_Final(md.data(), &ctx);

    std::string sPwhashSession;
    boost::algorithm::hex(md.begin(), md.end(), back_inserter(sPwhashSession));

    return sChecked_ == sPwhashSession;
}

#ifdef UNIX
void UNIXSignal(int sg_) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->info("Receive stopped signal...");
    if (g_pS2HB != nullptr) {
        g_pS2HB->shutdown();
    }
    logger->info("Stopped signal processed");
}
#else
bool WinSignal(DWORD SigType_) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    logger->info("Receive stopped signal...");

    switch (SigType_) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        logger->info("Interrupted by keyboard");
        break;
    case CTRL_CLOSE_EVENT:
        logger->info("Interrupted by Close");
        break;
    case CTRL_LOGOFF_EVENT:
        logger->info("Interrupted by LogOff");
        break;
    case CTRL_SHUTDOWN_EVENT:
        logger->info("Interrupted by shutdown");
        break;
    default:
        logger->info("Interrupted by unknown signal");
        break;
    }

    if (g_pS2HB != nullptr) {
        g_pS2HB->shutdown();
    }

    logger->info("Stopped signal processed");
    return true;
}

void WINAPI ServiceControl(DWORD dwControlCode) {
    switch (dwControlCode) {
    case SERVICE_CONTROL_STOP:
        g_pS2HB->report_service_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
        g_pS2HB->shutdown();
        g_pS2HB->report_service_status(SERVICE_STOPPED, NOERROR, 0);
        break;
    case SERVICE_CONTROL_INTERROGATE:
        g_pS2HB->report_service_status(g_pS2HB->get_ss_current_state(), NOERROR, 0);
        break;
    default:
        g_pS2HB->report_service_status(g_pS2HB->get_ss_current_state(), NOERROR, 0);
        break;
    }
}

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* plszArgv) {
    if (g_pS2HB != NULL) {
        char ServicePath[_MAX_PATH + 3];
        // Получаем путь к exe-файлу
        GetModuleFileName(NULL, ServicePath, _MAX_PATH);
        PathRemoveFileSpec(ServicePath);
        SetCurrentDirectory(ServicePath);

        SERVICE_STATUS_HANDLE ssHandle = RegisterServiceCtrlHandler(g_pS2HB->get_service_name(), ServiceControl);

        if (ssHandle != NULL) {
            g_pS2HB->set_ss_handle(ssHandle);
        }

        g_pS2HB->report_service_status(SERVICE_RUNNING, NO_ERROR, 0);

        if (g_pS2HB->prepare_to_start() && g_pS2HB->auto_execute() && g_pS2HB->startup()) {
            g_pS2HB->run();
        }

        g_pS2HB->finish();
    }
}

void server2_hb::report_service_status(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_) {
    static DWORD dwCheckPoint = 1;

    if (dwCurrentState_ == SERVICE_START_PENDING) {
        m_service_status.dwControlsAccepted = 0;
    } else {
        m_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }
    m_service_status.dwCurrentState = dwCurrentState_;
    m_service_status.dwWin32ExitCode = dwWin32ExitCode_;
    m_service_status.dwWaitHint = dwWaitHint_;

    if ((dwCurrentState_ == SERVICE_RUNNING) ||
        (dwCurrentState_ == SERVICE_STOPPED)) {
        m_service_status.dwCheckPoint = 0;
    } else {
        m_service_status.dwCheckPoint = dwCheckPoint++;
    }
    SetServiceStatus(m_service_status_handle, &m_service_status);
}

bool server2_hb::service_install() {
    auto logger = get_logger();
    logger->debug("ServiceInstalling...");

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        logger->warn("ServiceInstall: Не удалось открыть SC Manager");
        return false;
    }

    TCHAR ServicePath[_MAX_PATH + 3];

    // Получаем путь к exe-файлу
    GetModuleFileName(NULL, ServicePath + 1, _MAX_PATH);

    // Заключаем в кавычки
    ServicePath[0] = TEXT('\"');
    ServicePath[lstrlen(ServicePath) + 1] = 0;
    ServicePath[lstrlen(ServicePath)] = TEXT('\"');
    lstrcat(ServicePath, " --service --cfg ");
    lstrcat(ServicePath, m_sConfigFileName.c_str());

    logger->debug(ServicePath);

    // Создаём службу
    hService = CreateService(
        hSCManager,
        m_service_name.c_str(),
        m_service_display_name.c_str(),
        0,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        ServicePath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);

    CloseServiceHandle(hSCManager);

    if (!hService) {
        logger->warn("ServiceInstall: Не удалось создать службу");
        return false;
    }

    CloseServiceHandle(hService);
    logger->debug("ServiceInstalled");
    return true;
};

bool server2_hb::service_uninstall() {
    auto logger = get_logger();
    logger->debug("ServiceUninstall...");

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        logger->warn("ServiceUninstall: Не удалось открыть SC Manager");
        return false;
    }

    hService = OpenService(hSCManager, m_service_name.c_str(), DELETE | SERVICE_STOP);
    if (!hService) {
        logger->warn("ServiceUninstall: Не удалось открыть службу");
        CloseServiceHandle(hSCManager);
        return true;
    }

    SERVICE_STATUS ss;

    ControlService(hService, SERVICE_CONTROL_STOP, &ss);

    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    logger->debug("ServiceUninstalled");
    return true;
};

bool server2_hb::service_start() {
    auto logger = get_logger();
    logger->debug("ServiceStarting...");

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        logger->warn("ServiceStart: no open SC Manager");
        return false;
    }

    hService = OpenService(hSCManager, m_service_name.c_str(), SERVICE_START);
    if (!hService) {
        logger->warn("ServiceStart: not open service");
        CloseServiceHandle(hSCManager);
        return true;
    }
    if (!StartService(hService, 0, NULL)) {
        logger->warn("ServiceStart: not started");
        return false;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    logger->debug("ServiceStarted");
    return true;
};

bool server2_hb::service_stop() {
    auto logger = get_logger();
    logger->debug("Service Stopping...");

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        logger->warn("ServiceStop: Не удалось открыть SC Manager");
        return false;
    }

    hService = OpenService(hSCManager, m_service_name.c_str(), SERVICE_STOP);
    if (!hService) {
        logger->warn("ServiceStop: Не удалось открыть службу");
        CloseServiceHandle(hSCManager);
        return true;
    }

    SERVICE_STATUS ss;

    ControlService(hService, SERVICE_CONTROL_STOP, &ss);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    logger->debug("Service Stopped...");
    return true;
};

#define SVC_ERROR                        ((DWORD)0xC0020001L)

void server2_hb::service_report_event(LPTSTR szFunction) {
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, m_service_name.c_str());

    if (NULL != hEventSource) {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = m_service_name.c_str();
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

#endif  // UNIX


server2_hb::server2_hb() :
    m_connections(reinterpret_cast<connection_context*>(this))
    , m_db_base(m_db_connect_)
    , m_nHTTPListenPort(0)
    , m_dispatcher(this)
    , m_shutdown_timer(false) {
}

bool server2_hb::shutdown() {
    m_shutdown_timer = true;
    return m_connections.shutdown();
}

bool server2_hb::prepare_to_start() {
    auto logger = get_logger();
    logger->debug("[SERVER] Prepare to start");

    network::init();

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &server2_hb::action_auth));
    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>("refresh_token", &server2_hb::action_refresh_token));

    set_listen_port(m_configure_.get_value("http:port", S2H_DEFAULT_TCP_PORT));
    set_http_doc_root(m_configure_.get_value("http:docroot", "./"), m_configure_.get_value("http:storageroot", "./"));
    set_session_driver(m_configure_.get_value("http:session", "./session.db"));

    set_connect(
        m_configure_.get_value("storage:host", "localhost")
        , m_configure_.get_value("storage:port", 5432)
        , m_configure_.get_value("storage:db", "")
        , m_configure_.get_value("storage:user", "")
        , m_configure_.get_value("storage:password", "")
    );

    logger->debug("[GEOIP] MMDB version: {}", khorost::network::geo_ip_database::get_lib_version_mmdb());

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
    if (db.open_database(geoip_asn_path)){
//        logger->debug("[GEOIP] meta asn info \"{}\"", db.get_metadata());
        m_geoip_asn_path_ = geoip_asn_path;
    } else {
        logger->warn("[GEOIP] Error init MMDB ASN by path {}", geoip_asn_path);
    }

    return true;
}

bool server2_hb::auto_execute() {
    auto logger = get_logger();
    logger->info("[SERVER] AutoExecute");

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

bool server2_hb::check_params(int argc, char* argv[], int& result) {
    g_pS2HB = this;

    po::options_description desc("Allowable options");
    desc.add_options()
        ("name", po::value<std::string>(), "set service name")
        ("install", "install service")
        ("uninstall", "uninstall service")
        ("start", "start service")
        ("stop", "stop service")
        ("restart", "restart service")
        ("service", "run in service mode")
        ("configure", "configure server")
        ("cfg", po::value<std::string>(), "config file (default server.conf)")
        ("console", "run in console mode (default)")
        ;
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch (...) {
        std::cout << desc << std::endl;
        result = EXIT_FAILURE;
        return false;
    }

    po::notify(vm);

    m_sConfigFileName = std::string("configure.") + get_context_default_name() + std::string(".json");
    if (vm.count("cfg")) {
        m_sConfigFileName = vm["cfg"].as<std::string>();
    }

    auto console = spdlog::get(KHL_LOGGER_CONSOLE);
    if (m_configure_.load(m_sConfigFileName)) {
        console->debug("Config file parsed");
        khorost::log::prepare_logger(m_configure_, KHL_LOGGER_COMMON);
        khorost::log::prepare_logger(m_configure_, KHL_LOGGER_PROFILER);
    } else {
        console->warn("Config file not parsed");
        return false;
    }

    auto logger = get_logger();
#if defined(_WIN32) || defined(_WIN64)
    m_service_name = vm.count("name")?vm["name"].as<std::string>(): m_configure_.get_value("service:name", std::string("khlsrv_") + get_context_default_name());
    m_service_display_name = m_configure_.get_value("service:displayName", std::string("Khorost Service (") + get_context_default_name() + ")");

    if (vm.count("service")) {
        m_run_as_service = true;

        logger->info("Service = {}", m_service_name);
        SERVICE_TABLE_ENTRY DispatcherTable[] = {
            { (LPSTR)m_service_name.c_str(), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
            {nullptr, nullptr }
        };

        if (!StartServiceCtrlDispatcher(DispatcherTable)) {
            logger->info("InitAsService: Probable error 1063");
        };

        logger->info("Service shutdown");
        return false;
    }
    if (vm.count("configure")) {
        FreeConsole();
        call_server_configurator();
        return false;
    }
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return false;
    }
    // Управление сервисом
    if (vm.count("install")) {
        m_run_as_service = true;
        service_uninstall();
        service_install();
        logger->info("Install Service");
        return false;
    } else if (vm.count("start")) {
        m_run_as_service = true;
        service_start();
        logger->info("Start Service");
        return false;
    } else if (vm.count("stop")) {
        m_run_as_service = true;
        service_stop();
        logger->info("Stop Service");
        return false;
    } else if (vm.count("restart")) {
        m_run_as_service = true;
        service_stop();
        logger->info("Stop Service");
        service_start();
        logger->info("Start Service");
        return false;
    } else if (vm.count("uninstall")) {
        m_run_as_service = true;
        service_uninstall();
        logger->info("Uninstall Service");
        return false;
    }

    SetConsoleCtrlHandler((PHANDLER_ROUTINE)&WinSignal, TRUE);
#else
    setlocale(LC_ALL, "");

    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, UNIXSignal);
    signal(SIGINT, UNIXSignal);
#endif

    return true;
}

bool server2_hb::startup() {
    m_TimerThread.reset(new std::thread(boost::bind(&stub_timer_run, this)));
    return m_connections.start_listen(m_nHTTPListenPort, 5);
}

bool server2_hb::run() {
    m_TimerThread->join();
    return m_connections.wait_listen();
}

bool server2_hb::finish() {
    khorost::network::destroy();

    m_logger_ = nullptr;
    spdlog::shutdown();

    return true;
}

void server2_hb::http_connection::get_client_ip(char* pBuffer_, size_t buffer_size) {
    const char* pPIP = m_http->get_client_proxy_ip();
    if (pPIP != nullptr) {
        strncpy(pBuffer_, pPIP, buffer_size);
    } else {
        connection::get_client_ip(pBuffer_, buffer_size);
    }
}

bool server2_hb::process_http_action(const std::string& action, const std::string& uri_params, http_connection& connection) {
    auto http = connection.get_http();
    const auto s2_h_session = reinterpret_cast<network::s2h_session*>(processing_session(connection).get());

    const auto it = m_dictActionS2H.find(action);
    if (it != m_dictActionS2H.end()) {
        const auto func_action = it->second;
        return (this->*func_action)(uri_params, connection, s2_h_session);
    }
    return false;
}

void server2_hb::parse_action(const std::string& query, std::string& action, std::string& params) {
    const auto pos = query.find_first_of('/');
    if (pos != std::string::npos) {
        action = query.substr(0, pos);
        params = query.substr(pos + 1);
    } else {
        action = query;
        params.clear();
    }
}

bool server2_hb::process_http(http_connection& connection) {
    auto logger = get_logger();

    auto http = connection.get_http();
    const auto query_uri = http->get_query_uri();
    const auto url_prefix_action = get_url_prefix_action();
    const auto size_upa = strlen(url_prefix_action);

    logger->debug("[HTTP_PROCESS] URI='{}'", query_uri);

    if (strncmp(query_uri, url_prefix_action, size_upa) == 0) {
        std::string action, params;
        parse_action(query_uri + size_upa, action, params);
        if (process_http_action(action, params, connection)) {
            return true;
        }

        logger->debug("[HTTP_PROCESS] worker pQueryAction not found");

        http->set_response_status(http_response_status_not_found, "Not found");
        http->send_response(connection, "File not found");

        return false;
    }
    return process_http_file_server(query_uri, connection);
}

bool server2_hb::process_http_file_server(const std::string& query_uri, http_connection& connection ) {
    auto http = connection.get_http();

    const std::string prefix = get_url_prefix_storage();

    if (prefix == query_uri.substr(0, prefix.size())) {
        return http->send_file(query_uri.substr(prefix.size() - 1), connection, m_storage_root);
    }

    return http->send_file(query_uri, connection, m_doc_root);
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

    while (!server->m_shutdown_timer) {
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

void   server2_hb::set_session_driver(const std::string& driver) {
    m_sessions.open(driver, SESSION_VERSION_MIN, SESSION_VERSION_CURRENT, get_session_creator());
}

network::session_ptr server2_hb::processing_session(http_connection& connect) {
    using namespace boost::posix_time;

    auto logger = get_logger();
    auto http = connect.get_http();

    auto created = false;
    const auto session_id = http->get_cookie(get_session_code(), nullptr);
    auto sp = m_sessions.get_session(session_id != nullptr ? session_id : "", created, get_session_creator());
    auto* s2_h_session = reinterpret_cast<network::s2h_session*>(sp.get());

    char s_ip[255];
    connect.get_client_ip(s_ip, sizeof(s_ip));

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

network::token_ptr server2_hb::parse_token(const khorost::network::http_text_protocol_header_ptr& http, const bool is_access_token, const boost::posix_time::ptime& check) {
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
        if (check != boost::date_time::neg_infin && (is_access_token && !token->is_no_expire_access(check) || !is_access_token && !token->is_no_expire_refresh(check))) {
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

bool server2_hb::action_refresh_token(const std::string& params_uri, http_connection& connection, khorost::network::s2h_session* session) {
    const auto& logger = get_logger();
    const auto& http = connection.get_http();
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

            token->set_refresh_token( refresh_token);
            token->set_refresh_duration(time_refresh);
            token->set_refresh_expire(refresh_expire);

            const auto& payload = token->get_payload();
            const auto token_value = khorost::data::json_string(payload);
            const auto cache_set_value = payload[get_cache_set_tag()].asString();
            // clear previous state
            m_cache_db_.del({ prev_access_token , prev_refresh_token });
            m_cache_db_.srem(m_cache_db_context_ + "tt:" + cache_set_value, { prev_refresh_token });
            // set new state
            m_cache_db_.setex(m_cache_db_context_ + "at:" + access_token, time_access + khl_token_append_time,
                token_value);
            m_cache_db_.setex(m_cache_db_context_ + "rt:" + refresh_token, time_refresh + khl_token_append_time,
                token_value);
            m_cache_db_.sadd(m_cache_db_context_ + "tt:" + cache_set_value, { refresh_token });
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

bool server2_hb::action_auth(const std::string& uri_params, http_connection& connection, network::s2h_session* session) {
    using namespace boost::posix_time;

    auto http = connection.get_http();

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

void server2_hb::append_token(const khorost::network::token_ptr& token) {
    if (token != nullptr) {
        m_refresh_tokens.insert(std::make_pair(token->get_refresh_token(), token));
        m_access_tokens.insert(std::make_pair(token->get_access_token(), token));
    }
}

void server2_hb::remove_token(const bool is_access_token, const std::string& token_id) {
    if (is_access_token) {
        const auto it = m_access_tokens.find(token_id);
        if (it != m_access_tokens.end()) {
            const auto t = it->second;
            m_refresh_tokens.erase(t->get_refresh_token());
            m_access_tokens.erase(t->get_access_token());
        }
    } else {
        const auto it = m_refresh_tokens.find(token_id);
        if (it != m_refresh_tokens.end()) {
            const auto t = it->second;
            m_refresh_tokens.erase(t->get_refresh_token());
            m_access_tokens.erase(t->get_access_token());
        }
    }
}

void server2_hb::remove_token(const std::string& access_token, const std::string& refresh_token) {
    m_refresh_tokens.erase(refresh_token);
    m_access_tokens.erase(access_token);
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

    const auto token_context = m_cache_db_context_ + (is_access_token ? "at:" : "rt:") + token_id;
    const std::vector<std::string> token_context_vector = {token_context};
    auto rit = m_cache_db_.exists(token_context_vector);
    m_cache_db_.sync_commit();

    const auto exist = rit.get();
    if (!exist.is_null() && exist.as_integer() == 1) {
        if (is_access_token) {
            const auto it = m_access_tokens.find(token_id);
            if (it != m_access_tokens.end()) {
                return it->second;
            }
        } else {
            const auto it = m_refresh_tokens.find(token_id);
            if (it != m_refresh_tokens.end()) {
                return it->second;
            }
        }

        auto riv = m_cache_db_.get(token_context);
        m_cache_db_.sync_commit();

        const auto cp = riv.get();
        if (!cp.is_null()) {
            Json::Value payload;
            data::parse_json_string(cp.as_string(), payload);

            auto token = std::make_shared<network::token>(
                payload[khl_json_param_access_token].asString()
                , boost::posix_time::from_iso_extended_string(payload[khl_json_param_access_expire].asString())
                , payload[khl_json_param_refresh_token].asString()
                , boost::posix_time::from_iso_extended_string(payload[khl_json_param_refresh_expire].asString())
                , payload);

            append_token(token);
            return token;
        }
    } else {
        remove_token(is_access_token, token_id);
    }

    return nullptr;
}

std::shared_ptr<spdlog::logger> server2_hb::get_logger() {
    if (m_logger_ == nullptr) {
        m_logger_ = spdlog::get(KHL_LOGGER_COMMON);
    }
    return m_logger_;
}

std::shared_ptr<spdlog::logger> server2_hb::get_logger_profiler() {
    if (m_logger_profiler_ == nullptr) {
        m_logger_profiler_ = spdlog::get(KHL_LOGGER_PROFILER);
    }
    return m_logger_profiler_;
}
