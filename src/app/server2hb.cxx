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

khorost::Server2HB* khorost::g_pS2HB = NULL;

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

void Server2HB::ReportServiceStatus(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_) {
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

bool Server2HB::ServiceInstall() {
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

bool Server2HB::ServiceUninstall() {
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

bool Server2HB::ServiceStart() {
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

bool Server2HB::ServiceStop() {
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

void Server2HB::ServiceReportEvent(LPTSTR szFunction) {
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


Server2HB::Server2HB() :
    m_Connections(reinterpret_cast<ConnectionContext*>(this))
    , m_dbBase(m_dbConnect)
    , m_nHTTPListenPort(0)
    , m_Dispatcher(this)
    , m_bShutdownTimer(false) {
}

bool Server2HB::Shutdown() {
    m_bShutdownTimer = true;
    return m_Connections.Shutdown();
}

bool Server2HB::PrepareToStart() {
    LOG(DEBUG) << "PrepareToStart";

    Network::Init();

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &Server2HB::action_auth));

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

bool Server2HB::AutoExecute() {
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

bool Server2HB::CheckParams(int argc_, char* argv_[], int& nResult_, g3::LogWorker* logger_) {
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

bool Server2HB::Startup() {
    m_TimerThread.reset(new boost::thread(boost::bind(&stubTimerRun, this)));
    return m_Connections.StartListen(m_nHTTPListenPort, 5);
}

bool Server2HB::Run() {
    m_TimerThread->join();
    return m_Connections.WaitListen();
}

bool Server2HB::Finish() {
    m_dbGeoIP.CloseDatabase();
    khorost::Network::Destroy();
    return true;
}

void Server2HB::HTTPConnection::GetClientIP(char* pBuffer_, size_t nBufferSize_) {
    const char* pPIP = m_HTTP.GetClientProxyIP();
    if (pPIP != nullptr) {
        strncpy(pBuffer_, pPIP, nBufferSize_);
    } else {
        Network::Connection::GetClientIP(pBuffer_, nBufferSize_);
    }
}

bool Server2HB::process_http_action(const std::string& action, const std::string& uri_params, Network::S2HSession* session,
    HTTPConnection& connection, Network::HTTPTextProtocolHeader& http) {
    const auto it = m_dictActionS2H.find(action);
    if (it != m_dictActionS2H.end()) {
        const auto func_action = it->second;
        return (this->*func_action)(uri_params, connection, session, http);
    }
    return false;
}

bool Server2HB::ProcessHTTP(HTTPConnection& connection) {
    auto& http = connection.GetHTTP();
    auto session = ProcessingSession(connection, http);
    auto s2_h_session = reinterpret_cast<Network::S2HSession*>(session.get());
    const auto query_uri = http.GetQueryURI();
    const auto url_prefix_action = GetURLPrefixAction();
    const auto size_upa = strlen(url_prefix_action);

    LOGF(DEBUG, "[HTTP_PROCESS] URI='%s'", query_uri);

    if (strncmp(query_uri, url_prefix_action, size_upa) == 0) {
        std::string query_uri_action(query_uri + size_upa);
        const auto pos = query_uri_action.find_first_of('/');
        if (pos != std::string::npos) {
            if (process_http_action(query_uri_action.substr(0, pos), query_uri_action.substr(pos + 1), s2_h_session,
                                    connection, http)) {
                return true;
            }
        } else {
            if (process_http_action(query_uri_action, "", s2_h_session, connection, http)) {
                return true;
            }
        }

        LOG(DEBUG) << "[HTTP_PROCESS] worker pQueryAction not found";

        http.SetResponseStatus(404, "Not found");
        http.Response(connection, "File not found");

        return false;
    }
    return ProcessHTTPFileServer(query_uri, s2_h_session, connection, http);
}

bool Server2HB::ProcessHTTPFileServer(const std::string& sQueryURI_, Network::S2HSession* sp_, HTTPConnection& rConnect_, Network::HTTPTextProtocolHeader& rHTTP_) {
    Network::HTTPFileTransfer    hft;

    const std::string prefix = GetURLPrefixStorage();

    if (prefix == sQueryURI_.substr(0, prefix.size())) {
        return hft.SendFile(sQueryURI_.substr(prefix.size() - 1), rConnect_, rHTTP_, m_sStorageRoot);
    } else {
        return hft.SendFile(sQueryURI_, rConnect_, rHTTP_, m_strDocRoot);
    }
}

void Server2HB::TimerSessionUpdate() {
    Network::ListSession ls;

    m_Sessions.CheckAliveSessions();

    if (m_Sessions.GetActiveSessionsStats(ls)) {
        m_dbBase.SessionUpdate(ls);
    }
}

void Server2HB::stubTimerRun(Server2HB* pThis_) {
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

Network::SessionPtr Server2HB::ProcessingSession(HTTPConnection& connection, Network::HTTPTextProtocolHeader& http) {
    using namespace boost::posix_time;

    bool        bCreated = false;
    const char* pQuerySession = http.GetCookie(GetSessionCode());
    Network::SessionPtr  sp = m_Sessions.GetSession(pQuerySession != nullptr ? pQuerySession : "", bCreated);
    Network::S2HSession* s2hSession = reinterpret_cast<Network::S2HSession*>(sp.get());

    char sIP[255];
    connection.GetClientIP(sIP, sizeof(sIP));

    if (s2hSession != nullptr) {
        s2hSession->SetLastActivity(second_clock::universal_time());
        s2hSession->SetIP(sIP);

        if (bCreated) {
            Json::Value    jvStats;

            const char* pHP = http.GetHeaderParameter(HTTP_ATTRIBUTE_USER_AGENT);
            if (pHP != nullptr) {
                jvStats["UserAgent"] = pHP;
            }
            pHP = http.GetHeaderParameter(HTTP_ATTRIBUTE_ACCEPT_ENCODING);
            if (pHP != nullptr) {
                jvStats["AcceptEncoding"] = pHP;
            }
            pHP = http.GetHeaderParameter(HTTP_ATTRIBUTE_ACCEPT_LANGUAGE);
            if (pHP != nullptr) {
                jvStats["AcceptLanguage"] = pHP;
            }
            pHP = http.GetHeaderParameter(HTTP_ATTRIBUTE_REFERER);
            if (pHP != nullptr) {
                jvStats["Referer"] = pHP;
            }
            m_dbBase.SessionLogger(s2hSession, jvStats);
        }
    }

    http.SetCookie(GetSessionCode(), sp->GetSessionID(), sp->get_expired(), http.GetHost(), true);

    LOGF(DEBUG, "%s = '%s' ClientIP = '%s' ConnectID = #%d InS = '%s' "
        , GetSessionCode(), sp->GetSessionID().c_str(), sIP, connection.GetID()
        , pQuerySession != NULL ? (strcmp(sp->GetSessionID().c_str(), pQuerySession) == 0 ? "+" : pQuerySession) : "-"
    );
    return sp;
}

bool Server2HB::action_auth(const std::string& uri_params, Network::Connection& connection, Network::S2HSession* session, Network::HTTPTextProtocolHeader& http) {
    using namespace boost::posix_time;

    LOGF(DEBUG, "[ActionAuth]");
    Json::Value jvRoot;
    std::string sLogin, sNickname, sPwHash, sSalt;
    int nUserID;

    const char* pActionParam = http.GetParameter(GetURLParamActionParam());
    if (pActionParam == nullptr) {
        Json::Reader reader;
        Json::Value auth;

        const auto pBody = reinterpret_cast<const char*>(http.GetBody());
        if (reader.parse(pBody, pBody + http.GetBodyLength(), auth)) {
            const auto login = auth["login"].asString();
            const auto password = auth["password"].asString();

            if (m_dbBase.get_user_info(login, nUserID, sNickname, sPwHash, sSalt)) {
                std::string calculate_passowrd_hash;
                RecalcPasswordHash(calculate_passowrd_hash, login, password, sSalt);
                if (calculate_passowrd_hash == sPwHash) {
                    session->SetUserID(nUserID);
                    session->SetNickname(sNickname);
                    // TODO: pSession->SetPostion(/*------* /);
                    session->SetAuthenticate(true);

                    m_dbBase.GetUserRoles(nUserID, session);
                    m_dbBase.SessionUpdate(session);

                    m_Sessions.UpdateSession(session);
                }
            }
        }
    } else {
        LOGF(DEBUG, "[ActionAuth] ActionParam = '%s'", pActionParam);

        const auto pQueryQ = http.GetParameter(S2H_PARAM_QUESTION);
        if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_PRE) == 0) {
            if (m_dbBase.get_user_info(pQueryQ != nullptr ? pQueryQ : "", nUserID, sNickname, sPwHash, sSalt)) {
                jvRoot[S2H_JSON_SALT] = sSalt;
            } else {
                // не найден пользователь. но так в принципе наверно не стоит 
                LOGF(WARNING, "User not found");
                // посылаем fake данные к проверке подключения
                jvRoot[S2H_JSON_SALT] = session->GetSessionID();
            }
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_DO) == 0) {

            if (m_dbBase.get_user_info(pQueryQ != nullptr ? pQueryQ : "", nUserID, sNickname, sPwHash, sSalt)) {
                if (IsPasswordHashEqual2(http.GetParameter(S2H_PARAM_HASH, ""), sPwHash, session->GetSessionID())) {
                    session->SetUserID(nUserID);
                    session->SetNickname(sNickname);
                    // TODO: pSession->SetPostion(/*------* /);
                    session->SetAuthenticate(true);

                    m_dbBase.GetUserRoles(nUserID, session);
                    m_dbBase.SessionUpdate(session);
                    m_Sessions.UpdateSession(session);
                } else {
                    // не найден пользователь
                    LOGF(WARNING, "Password not correct");
                }
            } else {
                // не найден пользователь
                LOGF(WARNING, "User not found");
            }
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_RESET) == 0) {
            session->reset();
            session->set_expired(second_clock::universal_time());

            http.SetCookie(GetSessionCode(), session->GetSessionID(), session->get_expired(), http.GetHost(),true);

            m_dbBase.SessionUpdate(session);
            m_Sessions.RemoveSession(session);
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_CHANGEPASS) == 0) {

            std::string sAdminPassword = http.GetParameter(S2H_PARAM_ADMIN_PASSWORD, "");
            if (!sAdminPassword.empty()
                && m_dbBase.GetUserInfo(session->GetUserID(), sLogin, sNickname, sPwHash, sSalt)
                && IsPasswordHashEqual3(sPwHash, sLogin, sAdminPassword, sSalt)) {

                std::string sNewPassword = http.GetParameter(S2H_PARAM_PASSWORD, "");
                if (http.IsParameterExist(S2H_PARAM_LOGIN_PASSWORD)) {
                    sLogin = http.GetParameter(S2H_PARAM_LOGIN_PASSWORD, "");
                    if (m_dbBase.get_user_info(sLogin, nUserID, sNickname, sPwHash, sSalt)) {
                        RecalcPasswordHash(sPwHash, sLogin, sNewPassword, sSalt);
                        m_dbBase.UpdatePassword(nUserID, sPwHash);
                    } else {
                        // пользователь не найден
                    }
                } else {
                    RecalcPasswordHash(sPwHash, sLogin, sNewPassword, sSalt);
                    m_dbBase.UpdatePassword(session->GetUserID(), sPwHash);
                }
            } else {
                jvRoot[S2H_JSON_REASON] = "UserNotFound";
            }
        }
        // if (strcmp(pActionParam, PMX_PARAM_ACTION_AUTH_CHECK)==0) - пройдет по умолчанию и передаст информацию об авторизации
    }

    json_fill_auth(session, true, jvRoot);

    http.SetContentType(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS);
    http.Response(connection, json_string(jvRoot, http.GetParameter(S2H_PARAM_HUMMAN_JSON, 0) == 1));

    return true;
}

std::string Server2HB::json_string(const Json::Value& value, bool styled) {
    if (styled) {
        return Json::StyledWriter().write(value);
    }
    return Json::FastWriter().write(value);
}

void Server2HB::json_fill_auth(Network::S2HSession* session, bool full_info, Json::Value& value) {
    value[S2H_JSON_AUTH] = session->IsAuthenticate();
    if (full_info && session->IsAuthenticate()) {
        value[S2H_JSON_NICKNAME] = session->GetNickname();
        Json::Value jvRoles;
        session->fill_roles(jvRoles);
        value[S2H_JSON_ROLES] = jvRoles;
    }
}

