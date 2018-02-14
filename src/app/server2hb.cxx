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
using namespace khorost::Network;

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
    if (khorost::g_pS2HB != NULL) {
        khorost::g_pS2HB->Shutdown();
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
    if (khorost::g_pS2HB != NULL) {
        khorost::g_pS2HB->Shutdown();
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

Server2HB::~Server2HB() {
}

bool Server2HB::Shutdown() {
    m_bShutdownTimer = true;
    return m_Connections.Shutdown();
}

bool Server2HB::PrepareToStart() {
    LOG(DEBUG) << "PrepareToStart";

    khorost::Network::Init();

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &Server2HB::ActionAuth));

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
    if (pPIP != NULL) {
        strncpy(pBuffer_, pPIP, nBufferSize_);
    } else {
        Network::Connection::GetClientIP(pBuffer_, nBufferSize_);
    }
}

bool Server2HB::ProcessHTTP(HTTPConnection& rConnect_) {
    HTTPTextProtocolHeader&     rHTTP = rConnect_.GetHTTP();
    SessionPtr                  sp = ProcessingSession(rConnect_, rHTTP);
    S2HSession*                 s2hSession = reinterpret_cast<S2HSession*>(sp.get());
    const char*                 pQueryURI = rHTTP.GetQueryURI();

    LOGF(DEBUG, "[HTTP_PROCESS] URI='%s'", pQueryURI);

    if (strcmp(pQueryURI, GetURLPrefixAction())==0) {
        const char* pQueryAction = rHTTP.GetParameter(GetURLParamAction());

        if (pQueryAction != NULL) {
            LOG(DEBUG) << "[HTTP_PROCESS] pQueryAction=" << pQueryAction;
            if (ProcessHTTPCommand(pQueryAction, s2hSession, rConnect_, rHTTP)) {
                return true;
            }
        }

        LOG(DEBUG) << "[HTTP_PROCESS] worker pQueryAction not found";

        rHTTP.SetResponseStatus(404, "Not found");
        rHTTP.Response(rConnect_, "File not found");

        return false;
    } else {
        return ProcessHTTPFileServer(pQueryURI, s2hSession, rConnect_, rHTTP);
    }
}

bool Server2HB::ProcessHTTPCommand(const std::string& sQueryAction_, Network::S2HSession* sp_, HTTPConnection& rConnect_, Network::HTTPTextProtocolHeader& rHTTP_) {
    DictionaryActionS2H::iterator  it = m_dictActionS2H.find(sQueryAction_);
    if (it != m_dictActionS2H.end() ){
        funcActionS2H fAction = it->second;
        return (this->*fAction)(rConnect_, sp_, rHTTP_);
    }
    return false;
}

bool Server2HB::ProcessHTTPFileServer(const std::string& sQueryURI_, Network::S2HSession* sp_, HTTPConnection& rConnect_, HTTPTextProtocolHeader& rHTTP_) {
    HTTPFileTransfer    hft;

    const std::string prefix = GetURLPrefixStorage();

    if (prefix == sQueryURI_.substr(0, prefix.size())) {
        return hft.SendFile(sQueryURI_.substr(prefix.size() - 1), rConnect_, rHTTP_, m_sStorageRoot);
    } else {
        return hft.SendFile(sQueryURI_, rConnect_, rHTTP_, m_strDocRoot);
    }
}

void Server2HB::TimerSessionUpdate() {
    khorost::Network::ListSession ls;

    m_Sessions.CheckAliveSessions();

    if (m_Sessions.GetActiveSessionsStats(ls)) {
        m_dbBase.SessionUpdate(ls);
    }
}

void Server2HB::stubTimerRun(Server2HB* pThis_) {
    using namespace boost;
    using namespace boost::posix_time;

    ptime   ptSessionUpdate, ptSessionIPUpdate;

    ptSessionUpdate = ptSessionIPUpdate = second_clock::universal_time();

    while (!pThis_->m_bShutdownTimer) {
        ptime  ptNow = second_clock::universal_time();
        if ((ptNow - ptSessionUpdate).minutes() >= 10) {
            LOG(DEBUG) << "Every 10 minutes check";
            ptSessionUpdate = ptNow;
            pThis_->TimerSessionUpdate();
        }
        if ((ptNow - ptSessionIPUpdate).hours() >= 1) {
            LOG(DEBUG) << "Every Hours check";
            ptSessionIPUpdate = ptNow;
            pThis_->m_dbBase.SessionIPUpdate();
        }

        this_thread::sleep_for(chrono::milliseconds(1000));
    }
    // сбросить кэш
    pThis_->TimerSessionUpdate();
}

SessionPtr Server2HB::ProcessingSession(HTTPConnection& rConnect_, HTTPTextProtocolHeader& rHTTP_) {
    using namespace boost::posix_time;

    bool        bCreated = false;
    const char* pQuerySession = rHTTP_.GetCookie(GetSessionCode());
    SessionPtr  sp = m_Sessions.GetSession(pQuerySession != NULL ? pQuerySession : "", bCreated);
    S2HSession* s2hSession = reinterpret_cast<S2HSession*>(sp.get());

    char sIP[255];
    rConnect_.GetClientIP(sIP, sizeof(sIP));

    if (s2hSession != NULL) {
        s2hSession->SetLastActivity(second_clock::universal_time());
        s2hSession->SetIP(sIP);

        if (bCreated) {
            Json::Value    jvStats;

            const char* pHP = rHTTP_.GetHeaderParameter(HTTP_ATTRIBUTE_USER_AGENT);
            if (pHP != NULL) {
                jvStats["UserAgent"] = pHP;
            }
            pHP = rHTTP_.GetHeaderParameter(HTTP_ATTRIBUTE_ACCEPT_ENCODING);
            if (pHP != NULL) {
                jvStats["AcceptEncoding"] = pHP;
            }
            pHP = rHTTP_.GetHeaderParameter(HTTP_ATTRIBUTE_ACCEPT_LANGUAGE);
            if (pHP != NULL) {
                jvStats["AcceptLanguage"] = pHP;
            }
            pHP = rHTTP_.GetHeaderParameter(HTTP_ATTRIBUTE_REFERER);
            if (pHP != NULL) {
                jvStats["Referer"] = pHP;
            }
            m_dbBase.SessionLogger(s2hSession, jvStats);
        }
    }

    rHTTP_.SetCookie(GetSessionCode(), sp->GetSessionID(), sp->GetExpired(), rHTTP_.GetHost());

    LOGF(DEBUG, "%s = '%s' ClientIP = '%s' ConnectID = #%d InS = '%s' "
        , GetSessionCode(), sp->GetSessionID().c_str(), sIP, rConnect_.GetID()
        , pQuerySession != NULL ? (strcmp(sp->GetSessionID().c_str(), pQuerySession) == 0 ? "+" : pQuerySession) : "-"
    );
    return sp;
}

bool Server2HB::ActionAuth(Network::Connection& rConnect_, S2HSession* pSession_, HTTPTextProtocolHeader& rHTTP_) {
    using namespace boost::posix_time;

    LOGF(DEBUG, "[ActionAuth]");
    Json::Value jvRoot;

    const char* pActionParam = rHTTP_.GetParameter(GetURLParamActionParam());
    if (pActionParam != NULL) {
        LOGF(DEBUG, "[ActionAuth] ActionParam = '%s'", pActionParam);
        std::string sLogin, sNickname, sPwHash, sSalt;
        int nUserID;

        const char* pQueryQ = rHTTP_.GetParameter(S2H_PARAM_QUESTION);
        if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_PRE) == 0) {
            if (m_dbBase.GetUserInfo(pQueryQ != NULL ? pQueryQ : "", nUserID, sNickname, sPwHash, sSalt)) {
                jvRoot[S2H_JSON_SALT] = sSalt;
            } else {
                // не найден пользователь. но так в принципе наверно не стоит 
                LOGF(WARNING, "User not found");
                // посылаем fake данные к проверке подключения
                jvRoot[S2H_JSON_SALT] = pSession_->GetSessionID();
            }
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_DO) == 0) {

            if (m_dbBase.GetUserInfo(pQueryQ != NULL ? pQueryQ : "", nUserID, sNickname, sPwHash, sSalt)) {
                if (IsPasswordHashEqual2(rHTTP_.GetParameter(S2H_PARAM_HASH, ""), sPwHash, pSession_->GetSessionID())) {
                    pSession_->SetUserID(nUserID);
                    pSession_->SetNickname(sNickname);
                    // TODO: pSession->SetPostion(/*------* /);
                    pSession_->SetAuthenticate(true);

                    m_dbBase.GetUserRoles(nUserID, pSession_);
                    m_dbBase.SessionUpdate(pSession_);
                    m_Sessions.UpdateSession(pSession_);
                } else {
                    // не найден пользователь
                    LOGF(WARNING, "Password not correct");
                }
            } else {
                // не найден пользователь
                LOGF(WARNING, "User not found");
            }
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_RESET) == 0) {
            pSession_->Reset();
            pSession_->SetExpired(second_clock::universal_time());
            rHTTP_.SetCookie(GetSessionCode(), pSession_->GetSessionID(), pSession_->GetExpired(), rHTTP_.GetHost());
            m_dbBase.SessionUpdate(pSession_);
            m_Sessions.RemoveSession(pSession_);
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_CHANGEPASS) == 0) {

            std::string sAdminPassword = rHTTP_.GetParameter(S2H_PARAM_ADMIN_PASSWORD, "");
            if (!sAdminPassword.empty()
                && m_dbBase.GetUserInfo(pSession_->GetUserID(), sLogin, sNickname, sPwHash, sSalt)
                && IsPasswordHashEqual3(sPwHash, sLogin, sAdminPassword, sSalt)) {

                std::string sNewPassword = rHTTP_.GetParameter(S2H_PARAM_PASSWORD, "");
                if (rHTTP_.IsParameterExist(S2H_PARAM_LOGIN_PASSWORD)) {
                    sLogin = rHTTP_.GetParameter(S2H_PARAM_LOGIN_PASSWORD, "");
                    if (m_dbBase.GetUserInfo(sLogin, nUserID, sNickname, sPwHash, sSalt)) {
                        RecalcPasswordHash(sPwHash, sLogin, sNewPassword, sSalt);
                        m_dbBase.UpdatePassword(nUserID, sPwHash);
                    } else {
                        // пользователь не найден
                    }
                } else {
                    RecalcPasswordHash(sPwHash, sLogin, sNewPassword, sSalt);
                    m_dbBase.UpdatePassword(pSession_->GetUserID(), sPwHash);
                }
            } else {
                jvRoot[S2H_JSON_REASON] = "UserNotFound";
            }
        }
        // if (strcmp(pActionParam, PMX_PARAM_ACTION_AUTH_CHECK)==0) - пройдет по умолчанию и передаст информацию об авторизации
    }

    JSON_FillAuth(pSession_, true, jvRoot);

    rHTTP_.SetContentType(HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS);
    rHTTP_.Response(rConnect_, JSONWrite(jvRoot, rHTTP_.GetParameter(S2H_PARAM_HUMMAN_JSON, 0) == 1));

    return true;
}

void Server2HB::JSON_PingPong(Network::HTTPTextProtocolHeader& rHTTP_, Json::Value& jvRoot_) const {
    bool bExist = false;
    const char* pPing = rHTTP_.GetParameter(S2H_PARAM_ACTION_PING, &bExist);
    if (bExist) {
        jvRoot_[S2H_JSON_PONG] = pPing;
    }
}

std::string Server2HB::JSONWrite(const Json::Value& jvRoot_, bool bStyled_) const {
    if (bStyled_) {
        return Json::StyledWriter().write(jvRoot_);
    } else {
        return Json::FastWriter().write(jvRoot_);
    }
}

bool Server2HB::JSON_FillAuth(Network::S2HSession* pSession_, bool bFullInfo_, Json::Value& jvRoot_) const {
    jvRoot_[S2H_JSON_AUTH] = pSession_->IsAuthenticate();
    if (bFullInfo_ && pSession_->IsAuthenticate()) {
        jvRoot_[S2H_JSON_NICKNAME] = pSession_->GetNickname();
        Json::Value jvRoles;
        pSession_->FillRoles(jvRoles);
        jvRoot_[S2H_JSON_ROLES] = jvRoles;
    }

    return true;
}

