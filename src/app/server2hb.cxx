#if defined(_WIN32) || defined(_WIN64)
 #include <windows.h>
 #include <shlwapi.h>
 #include <strsafe.h>
#endif

#include "app/server2hb.h"

#include <openssl/md5.h>
#include <boost/algorithm/hex.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

khorost::server2_hb* khorost::g_pS2HB = NULL;

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
    LOGF(INFO, "Receive stopped signal...");
    if (g_pS2HB != nullptr) {
        g_pS2HB->Shutdown();
    }
    LOGF(INFO, "Stopped signal processed");
}
#else
bool WinSignal(DWORD SigType_) {
    LOGF(INFO, "Receive stopped signal...");
    switch (SigType_) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        LOGF(INFO, "Interrupted by keyboard");
        break;
    case CTRL_CLOSE_EVENT:
        LOGF(INFO, "Interrupted by Close");
        break;
    case CTRL_LOGOFF_EVENT:
        LOGF(INFO, "Interrupted by LogOff");
        break;
    case CTRL_SHUTDOWN_EVENT:
        LOGF(INFO, "Interrupted by Shutdown");
        break;
    default:
        LOGF(INFO, "Interrupted by unknown signal");
        break;
    }
    if (g_pS2HB != nullptr) {
        g_pS2HB->Shutdown();
    }
    LOGF(INFO, "Stopped signal processed");
    return true;
}

void WINAPI ServiceControl(DWORD dwControlCode) {
    switch (dwControlCode) {
    case SERVICE_CONTROL_STOP:
        g_pS2HB->ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        g_pS2HB->Shutdown();
        g_pS2HB->ReportServiceStatus(SERVICE_STOPPED, NOERROR, 0);
        break;
    case SERVICE_CONTROL_INTERROGATE:
        g_pS2HB->ReportServiceStatus(g_pS2HB->GetSSCurrentState(), NOERROR, 0);
        break;
    default:
        g_pS2HB->ReportServiceStatus(g_pS2HB->GetSSCurrentState(), NOERROR, 0);
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

        SERVICE_STATUS_HANDLE ssHandle = RegisterServiceCtrlHandler(g_pS2HB->GetServiceName(), ServiceControl);

        if (ssHandle != NULL) {
            g_pS2HB->SetSSHandle(ssHandle);
        }

        g_pS2HB->ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

        if (g_pS2HB->PrepareToStart() && g_pS2HB->AutoExecute() && g_pS2HB->Startup()) {
            g_pS2HB->Run();
        }

        g_pS2HB->Finish();
    }
}

void server2_hb::ReportServiceStatus(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_) {
    static DWORD dwCheckPoint = 1;

    if (dwCurrentState_ == SERVICE_START_PENDING) {
        m_ss.dwControlsAccepted = 0;
    } else {
        m_ss.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }
    m_ss.dwCurrentState = dwCurrentState_;
    m_ss.dwWin32ExitCode = dwWin32ExitCode_;
    m_ss.dwWaitHint = dwWaitHint_;

    if ((dwCurrentState_ == SERVICE_RUNNING) ||
        (dwCurrentState_ == SERVICE_STOPPED)) {
        m_ss.dwCheckPoint = 0;
    } else {
        m_ss.dwCheckPoint = dwCheckPoint++;
    }
    SetServiceStatus(m_ssHandle, &m_ss);
}

bool server2_hb::ServiceInstall() {
    LOG(DEBUG) << "ServiceInstalling...";

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        LOG(WARNING) << "ServiceInstall: Не удалось открыть SC Manager";
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

    LOG(DEBUG) << ServicePath;

    // Создаём службу
    hService = CreateService(
        hSCManager,
        m_sServiceName.c_str(),
        m_sServiceDisplayName.c_str(),
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
        LOG(WARNING) << "ServiceInstall: Не удалось создать службу";
        return false;
    }

    CloseServiceHandle(hService);
    LOG(DEBUG) << "ServiceInstalled";
    return true;
};

bool server2_hb::ServiceUninstall() {
    LOG(DEBUG) << "ServiceUninstall...";

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        LOG(WARNING) << "ServiceUninstall: Не удалось открыть SC Manager";
        return false;
    }

    hService = OpenService(hSCManager, m_sServiceName.c_str(), DELETE | SERVICE_STOP);
    if (!hService) {
        LOG(WARNING) << "ServiceUninstall: Не удалось открыть службу";
        CloseServiceHandle(hSCManager);
        return true;
    }

    SERVICE_STATUS ss;

    ControlService(hService, SERVICE_CONTROL_STOP, &ss);

    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    LOG(DEBUG) << "ServiceUninstalled";
    return true;
};

bool server2_hb::ServiceStart() {
    LOG(DEBUG) << "ServiceStarting...";

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        LOG(WARNING) << "ServiceStart: no open SC Manager";
        return false;
    }

    hService = OpenService(hSCManager, m_sServiceName.c_str(), SERVICE_START);
    if (!hService) {
        LOG(WARNING) << "ServiceStart: not open service";
        CloseServiceHandle(hSCManager);
        return true;
    }
    if (!StartService(hService, 0, NULL)) {
        LOG(WARNING) << "ServiceStart: not started";
        return false;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    LOG(DEBUG) << "ServiceStarted";
    return true;
};

bool server2_hb::ServiceStop() {
    LOG(DEBUG) << "Service Stopping...";

    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        LOG(WARNING) << "ServiceStop: Не удалось открыть SC Manager";
        return false;
    }

    hService = OpenService(hSCManager, m_sServiceName.c_str(), SERVICE_STOP);
    if (!hService) {
        LOG(WARNING) << "ServiceStop: Не удалось открыть службу";
        CloseServiceHandle(hSCManager);
        return true;
    }

    SERVICE_STATUS ss;

    ControlService(hService, SERVICE_CONTROL_STOP, &ss);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    LOG(DEBUG) << "Service Stopped...";
    return true;
};

#define SVC_ERROR                        ((DWORD)0xC0020001L)

void server2_hb::ServiceReportEvent(LPTSTR szFunction) {
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, m_sServiceName.c_str());

    if (NULL != hEventSource) {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = m_sServiceName.c_str();
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
    m_Connections(reinterpret_cast<connection_context*>(this))
    , m_dbBase(m_dbConnect)
    , m_nHTTPListenPort(0)
    , m_Dispatcher(this)
    , m_bShutdownTimer(false) {
}

bool server2_hb::Shutdown() {
    m_bShutdownTimer = true;
    return m_Connections.Shutdown();
}

bool server2_hb::PrepareToStart() {
    LOG(DEBUG) << "PrepareToStart";

    network::init();

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &server2_hb::action_auth));
    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>("refresh_token", &server2_hb::action_refresh_token));

    SetListenPort(m_Configure.GetValue("http:port", S2H_DEFAULT_TCP_PORT));
    SetHTTPDocRoot(m_Configure.GetValue("http:docroot", "./"), m_Configure.GetValue("http:storageroot", "./"));
    SetSessionDriver(m_Configure.GetValue("http:session", "./session.db"));

    SetConnect(
        m_Configure.GetValue("storage:host", "localhost")
        , m_Configure.GetValue("storage:port", 5432)
        , m_Configure.GetValue("storage:db", "")
        , m_Configure.GetValue("storage:user", "")
        , m_Configure.GetValue("storage:password", "")
    );

    m_dbGeoIP.OpenDatabase(m_Configure.GetValue("geoip:db", ""));

    return true;
}

bool server2_hb::AutoExecute() {
    LOG(DEBUG) << "AutoExecute";

    Config::Iterator    cfgCreateUser = m_Configure["autoexec"]["Create"]["User"];
    if (!cfgCreateUser.isNull()) {
        for (Config::Iterator::ArrayIndex k = 0; k < cfgCreateUser.size(); ++k) {
            Config::Iterator cfgUser = cfgCreateUser[k];

            if (!m_dbBase.IsUserExist(cfgUser["Login"].asString())) {
                m_dbBase.CreateUser(cfgUser);
            }
        }
    }

    return true;
}

bool server2_hb::CheckParams(int argc_, char* argv_[], int& nResult_, g3::LogWorker* logger_) {
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
        po::store(po::parse_command_line(argc_, argv_, desc), vm);
    } catch (...) {
        std::cout << desc << std::endl;
        nResult_ = EXIT_FAILURE;
        return false;
    }

    po::notify(vm);

    m_sConfigFileName = std::string("configure.") + GetContextDefaultName() + std::string(".json");
    if (vm.count("cfg")) {
        m_sConfigFileName = vm["cfg"].as<std::string>();
    }

    if (m_Configure.Load(m_sConfigFileName)) {
        LOG(INFO) << "Config file parsed";

        if (logger_ != NULL) {
            auto fs = logger_->addDefaultLogger(GetContextDefaultName(), m_Configure.GetValue("log:path", "./"), "s2");
            std::future<std::string> log_file_name = fs->call(&g3::FileSink::fileName);
            LOG(DEBUG) << "Append file log - \'" << log_file_name.get() << "\'";
        }
    } else {
        LOG(WARNING) << "Config file not parsed";
        return false;
    }
#if defined(_WIN32) || defined(_WIN64)
    m_sServiceName = vm.count("name")?vm["name"].as<std::string>(): m_Configure.GetValue("service:name", std::string("khlsrv_") + GetContextDefaultName());
    m_sServiceDisplayName = m_Configure.GetValue("service:displayName", std::string("Khorost Service (") + GetContextDefaultName() + ")");

    if (vm.count("service")) {
        m_bRunAsService = true;

        LOG(INFO) << "Service = " << m_sServiceName;
        SERVICE_TABLE_ENTRY DispatcherTable[] = {
            { (LPSTR)m_sServiceName.c_str(), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
            { NULL, NULL }
        };

        if (!StartServiceCtrlDispatcher(DispatcherTable)) {
            LOG(WARNING) << "InitAsService: Probable error 1063";
        };
        
        LOG(DEBUG) << "Service shutdown";
        return false;
    }
    if (vm.count("configure")) {
        FreeConsole();
        CallServerConfigurator();
        return false;
    }
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return false;
    }
    // Управление сервисом
    if (vm.count("install")) {
        m_bRunAsService = true;
        ServiceUninstall();
        ServiceInstall();
        LOG(INFO) << "Install Service";
        return false;
    } else if (vm.count("start")) {
        m_bRunAsService = true;
        ServiceStart();
        LOG(INFO) << "Start Service";
        return false;
    } else if (vm.count("stop")) {
        m_bRunAsService = true;
        ServiceStop();
        LOG(INFO) <<"Stop Service";
        return false;
    } else if (vm.count("restart")) {
        m_bRunAsService = true;
        ServiceStop();
        LOG(INFO) << "Stop Service";
        ServiceStart();
        LOG(INFO) << "Start Service";
        return false;
    } else if (vm.count("uninstall")) {
        m_bRunAsService = true;
        ServiceUninstall();
        LOG(INFO) << "Uninstall Service";
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

bool server2_hb::Startup() {
    m_TimerThread.reset(new boost::thread(boost::bind(&stubTimerRun, this)));
    return m_Connections.StartListen(m_nHTTPListenPort, 5);
}

bool server2_hb::Run() {
    m_TimerThread->join();
    return m_Connections.WaitListen();
}

bool server2_hb::Finish() {
    m_dbGeoIP.CloseDatabase();
    khorost::network::destroy();
    return true;
}

void server2_hb::http_connection::get_client_ip(char* pBuffer_, size_t nBufferSize_) {
    const char* pPIP = m_http->get_client_proxy_ip();
    if (pPIP != nullptr) {
        strncpy(pBuffer_, pPIP, nBufferSize_);
    } else {
        connection::get_client_ip(pBuffer_, nBufferSize_);
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
    auto http = connection.get_http();
    const auto query_uri = http->get_query_uri();
    const auto url_prefix_action = GetURLPrefixAction();
    const auto size_upa = strlen(url_prefix_action);

    LOGF(DEBUG, "[HTTP_PROCESS] URI='%s'", query_uri);

    if (strncmp(query_uri, url_prefix_action, size_upa) == 0) {
        std::string action, params;
        parse_action(query_uri + size_upa, action, params);
        if (process_http_action(action, params, connection)) {
            return true;
        }

        LOG(DEBUG) << "[HTTP_PROCESS] worker pQueryAction not found";

        http->set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        http->response(connection, "File not found");

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

void server2_hb::TimerSessionUpdate() {
    network::list_session ls;

    m_Sessions.CheckAliveSessions();

    if (m_Sessions.GetActiveSessionsStats(ls)) {
        m_dbBase.SessionUpdate(ls);
    }
}

void server2_hb::stubTimerRun(server2_hb* pThis_) {
    using namespace boost;
    using namespace posix_time;

    ptime   ptSessionUpdate, ptSessionIPUpdate;

    ptSessionUpdate = ptSessionIPUpdate = second_clock::universal_time();

    while (!pThis_->m_bShutdownTimer) {
        const auto now = second_clock::universal_time();

        if ((now - ptSessionUpdate).minutes() >= 10) {
            LOG(DEBUG) << "Every 10 minutes check";
            ptSessionUpdate = now;
            pThis_->TimerSessionUpdate();
        }
        if ((now - ptSessionIPUpdate).hours() >= 1) {
            LOG(DEBUG) << "Every Hours check";
            ptSessionIPUpdate = now;
            pThis_->m_dbBase.SessionIPUpdate();
        }

        this_thread::sleep_for(chrono::milliseconds(1000));
    }
    // сбросить кэш
    pThis_->TimerSessionUpdate();
}

server2_hb::func_creator server2_hb::get_session_creator() {
    return [](const std::string& session_id, boost::posix_time::ptime created,
        boost::posix_time::ptime expired) {
        return boost::make_shared<network::s2h_session>(
            session_id, created, expired);
    };
}

void   server2_hb::SetSessionDriver(const std::string& driver) {
    m_Sessions.open(driver, SESSION_VERSION_MIN, SESSION_VERSION_CURRENT, get_session_creator());
}

network::session_ptr server2_hb::processing_session(http_connection& connection) {
    using namespace boost::posix_time;

    auto http = connection.get_http();

    auto created = false;
    const auto session_id = http->get_cookie(get_session_code());
    network::session_ptr sp = m_Sessions.get_session(session_id != nullptr ? session_id : "", created,
                                                    get_session_creator());
    network::s2h_session* s2hSession = reinterpret_cast<network::s2h_session*>(sp.get());

    char sIP[255];
    connection.get_client_ip(sIP, sizeof(sIP));

    if (s2hSession != nullptr) {
        s2hSession->set_last_activity(second_clock::universal_time());
        s2hSession->set_ip(sIP);

        if (created) {
            Json::Value    jvStats;

            const char* pHP = http->get_header_parameter(HTTP_ATTRIBUTE_USER_AGENT);
            if (pHP != nullptr) {
                jvStats["UserAgent"] = pHP;
            }
            pHP = http->get_header_parameter(HTTP_ATTRIBUTE_ACCEPT_ENCODING);
            if (pHP != nullptr) {
                jvStats["AcceptEncoding"] = pHP;
            }
            pHP = http->get_header_parameter(HTTP_ATTRIBUTE_ACCEPT_LANGUAGE);
            if (pHP != nullptr) {
                jvStats["AcceptLanguage"] = pHP;
            }
            pHP = http->get_header_parameter(HTTP_ATTRIBUTE_REFERER);
            if (pHP != nullptr) {
                jvStats["Referer"] = pHP;
            }
            m_dbBase.SessionLogger(s2hSession, jvStats);
        }
    }

    http->set_cookie(get_session_code(), sp->get_session_id(), sp->get_expired(), http->get_host(), true);

    LOGF(DEBUG, "%s = '%s' ClientIP = '%s' ConnectID = #%d InS = '%s' "
        , get_session_code(), sp->get_session_id().c_str(), sIP, connection.GetID()
        , session_id != NULL ? (strcmp(sp->get_session_id().c_str(), session_id) == 0 ? "+" : session_id) : "-"
    );
    return sp;
}

network::token_ptr server2_hb::parse_token(khorost::network::http_text_protocol_header_ptr& http, const bool is_access_token, const boost::posix_time::ptime& check) {
    static const std::string token_mask = "token ";
    std::string id;

    const auto header_authorization = http->get_header_parameter(KHL_HTTP_PARAM__AUTHORIZATION);
    if (header_authorization != nullptr) {
        const auto token_id = data::escape_string(header_authorization);
        const auto token_pos = token_mask.size();

        if (token_id.size() <= token_pos || token_id.substr(0, token_pos) != token_mask) {
            return nullptr;
        }

        id = token_id.substr(token_pos);
    } else {
        id = data::escape_string(http->get_parameter("token", ""));
    }

    auto token = is_access_token ? find_access_token(id) : find_refresh_token(id);
    if (token != nullptr && check != boost::date_time::neg_infin && (is_access_token && !token->
        is_no_expire_access(check) || !is_access_token && !token->is_no_expire_refresh(check))) {
        remove_token(token);
        return nullptr;
    }

    return token;
}

void server2_hb::fill_json_token(const network::token_ptr& token, Json::Value& value) {
    value["access_token"] = token->get_access_token();
    value["refresh_token"] = token->get_refresh_token();
    KHL_SET_TIMESTAMP_MILLISECONDS(value, "access_expire", token->get_access_expire());
    KHL_SET_TIMESTAMP_MILLISECONDS(value, "refresh_expire", token->get_refresh_expire());
}

bool server2_hb::action_refresh_token(const std::string& params_uri, http_connection& connection, khorost::network::s2h_session* session) {
    auto http = connection.get_http();
    Json::Value json_root;
    const auto now = boost::posix_time::microsec_clock::universal_time();

    try {
        auto token = parse_token(http, false, now);
        if (token != nullptr) {
            const auto time_refresh = http->get_parameter("time_refresh", 0);
            const auto time_access = http->get_parameter("time_access", 0);

            const auto access_token = token->get_access_token();
            const auto refresh_token = token->get_refresh_token();

            if (m_dbBase.refresh_token(token, time_access, time_refresh)) {
                update_tokens(token, access_token, refresh_token);

                fill_json_token(token, json_root);
            }
        }
    } catch (const std::exception&) {
        http->set_response_status(402, "UNKNOWN_ERROR");
    }

    if (!json_root.isNull()) {
        KHL_SET_CPU_DURATION(json_root, KHL_JSON_PARAM__DURATION, now);

        http->set_content_type(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JSON);
        http->response(connection, json_string(json_root));
    } else {
        http->set_response_status(HTTP_RESPONSE_STATUS_UNAUTHORIZED, "Unauthorized");
        http->end_of_response(connection);
    }

    return true;
}

bool server2_hb::action_auth(const std::string& uri_params, http_connection& connection, network::s2h_session* session) {
    using namespace boost::posix_time;

    auto http = connection.get_http();

    Json::Value root;
    Json::Reader reader;
    std::string action, params;
    std::string nickname, hash, salt;
    int user_id;

    parse_action(uri_params, action, params);

    if (action == "login") {
        Json::Value auth;
        const auto body = reinterpret_cast<const char*>(http->get_body());

        if (reader.parse(body, body + http->get_body_length(), auth)) {
            const auto login = auth["login"].asString();
            const auto password = auth["password"].asString();

            if (m_dbBase.get_user_info(login, user_id, nickname, hash, salt)) {
                decltype(hash) calculate_passowrd_hash;

                RecalcPasswordHash(calculate_passowrd_hash, login, password, salt);
                if (calculate_passowrd_hash == hash) {
                    session->SetUserID(user_id);
                    session->SetNickname(nickname);
                    // TODO: pSession->SetPostion(/*------* /);
                    session->SetAuthenticate(true);

                    m_dbBase.GetUserRoles(user_id, session);
                    m_dbBase.SessionUpdate(session);

                    m_Sessions.update_session(session);
                }
            }
        }
        json_fill_auth(session, true, root);
    } else if (action == "logout") {
        session->reset();
        session->set_expired(second_clock::universal_time());

        http->set_cookie(get_session_code(), session->get_session_id(), session->get_expired(), http->get_host(), true);

        m_dbBase.SessionUpdate(session);
        m_Sessions.remove_session(session);
    } else if (action == "change") {
        std::string login;
        const auto current_password = http->get_parameter("curpwd");

        if (current_password != nullptr
            && m_dbBase.GetUserInfo(session->GetUserID(), login, nickname, hash, salt)
            && IsPasswordHashEqual3(hash, login, current_password, salt)) {

            const auto new_password = http->get_parameter("newpwd");
            if (http->IsParameterExist("loginpwd")) {
                login = http->get_parameter("loginpwd");

                if (m_dbBase.get_user_info(login, user_id, nickname, hash, salt)) {
                    RecalcPasswordHash(hash, login, new_password, salt);
                    m_dbBase.UpdatePassword(user_id, hash);
                } else {
                    // пользователь не найден
                }
            } else {
                RecalcPasswordHash(hash, login, new_password, salt);
                m_dbBase.UpdatePassword(session->GetUserID(), hash);
            }
        } else {
            root[S2H_JSON_REASON] = "UserNotFound";
        }
        json_fill_auth(session, true, root);
    }

    http->set_content_type(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS);
    http->response(connection, json_string(root));

    return true;
}

std::string server2_hb::json_string(const Json::Value& value, const bool styled) {
    if (styled) {
        return Json::StyledWriter().write(value);
    }
    return Json::FastWriter().write(value);
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

void server2_hb::remove_token(const std::string& access_token, const std::string& refresh_token) {
    m_refresh_tokens.erase(refresh_token);
    m_access_tokens.erase(access_token);
}

void server2_hb::update_tokens(const network::token_ptr& token, const std::string& access_token,
                               const std::string& refresh_token) {
    remove_token(access_token, refresh_token);
    append_token(token);
}

network::token_ptr server2_hb::find_refresh_token(const std::string& refresh_token) {
    const auto it = m_refresh_tokens.find(refresh_token);
    if (it!=m_refresh_tokens.end()) {
        return it->second;
    }

    auto token = m_dbBase.load_token(true, refresh_token);
    if (token!=nullptr) {
        append_token(token);
    }

    return token;
}

network::token_ptr server2_hb::find_access_token(const std::string& access_token) {
    const auto it = m_access_tokens.find(access_token);
    if (it != m_access_tokens.end()) {
        return it->second;
    }

    auto token = m_dbBase.load_token(false, access_token);
    if (token != nullptr) {
        append_token(token);
    }

    return token;
}

