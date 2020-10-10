// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shlwapi.h>
#include <strsafe.h>
#endif

#include "app/server2h.h"
#include "app/khl-define.h"

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
khorost::server2_h* khorost::g_pS2HB = nullptr;

server2_h::server2_h() :
    m_connections(reinterpret_cast<connection_context*>(this))
    , m_nHTTPListenPort(0) {
}

void server2_h::http_connection::get_client_ip(const khorost::network::http_text_protocol_header* http, char* buffer,
                                               const size_t buffer_size) {
    const auto cpi = http->get_client_proxy_ip();
    if (cpi != nullptr) {
        strncpy(buffer, cpi, buffer_size);
    } else {
        connection::get_client_ip(buffer, buffer_size);
    }
}

bool server2_h::prepare_to_start() {
    auto logger = get_logger();
    logger->debug("[SERVER] Prepare to start");

    network::init();

    set_listen_port(m_configure_.get_value("http:port", S2H_DEFAULT_TCP_PORT));
    set_http_doc_root(m_configure_.get_value("http:docroot", "./"), m_configure_.get_value("http:storageroot", "./"));

    return true;
}

bool server2_h::auto_execute() {
    auto logger = get_logger();
    logger->info("[SERVER] AutoExecute");
    return true;
}

bool server2_h::startup() {
    return m_connections.start_listen(m_nHTTPListenPort);
}

bool server2_h::run() {
    return m_connections.wait_listen();
}

bool server2_h::finish() {
    khorost::network::destroy();

    m_logger_ = nullptr;
    spdlog::shutdown();

    return true;
}

bool server2_h::shutdown() {
    return m_connections.shutdown();
}


bool server2_h::process_http(http_connection& connection, khorost::network::http_text_protocol_header* http) {
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

bool server2_h::process_http_action(const std::string& action, const std::string& uri_params,
                                    http_connection& connection, khorost::network::http_text_protocol_header* http) {
    const auto it = m_dictActionS2Hs.find(action);
    if (it != m_dictActionS2Hs.end()) {
        const auto func_action = it->second;
        return (this->*func_action)(uri_params, connection, http);
    }
    return false;
}

void server2_h::parse_action(const std::string& query, std::string& action, std::string& params) {
    const auto pos = query.find_first_of('/');
    if (pos != std::string::npos) {
        action = query.substr(0, pos);
        params = query.substr(pos + 1);
    } else {
        action = query;
        params.clear();
    }
}

bool server2_h::process_http_file_server(const std::string& query_uri, http_connection& connection,
                                         khorost::network::http_text_protocol_header* http) {
    return http->send_file(query_uri, connection, m_doc_root);
}

std::shared_ptr<spdlog::logger> server2_h::get_logger() {
    if (m_logger_ == nullptr) {
        m_logger_ = spdlog::get(KHL_LOGGER_COMMON);
    }
    return m_logger_;
}

std::shared_ptr<spdlog::logger> server2_h::get_logger_profiler() {
    if (m_logger_profiler_ == nullptr) {
        m_logger_profiler_ = spdlog::get(KHL_LOGGER_PROFILER);
    }
    return m_logger_profiler_;
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

void server2_h::report_service_status(DWORD dwCurrentState_, DWORD dwWin32ExitCode_, DWORD dwWaitHint_) {
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

bool server2_h::service_install() {
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

bool server2_h::service_uninstall() {
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

bool server2_h::service_start() {
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

bool server2_h::service_stop() {
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

void server2_h::service_report_event(LPTSTR szFunction) {
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, m_service_name.c_str());

    if (NULL != hEventSource) {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = m_service_name.c_str();
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource, // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0, // event category
                    SVC_ERROR, // event identifier
                    NULL, // no security identifier
                    2, // size of lpszStrings array
                    0, // no binary data
                    lpszStrings, // array of strings
                    NULL); // no binary data

        DeregisterEventSource(hEventSource);
    }
}

#endif  // UNIX

bool server2_h::check_params(int argc, char* argv[], int& result) {
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
        ("console", "run in console mode (default)");
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
    m_service_name = vm.count("name")
                         ? vm["name"].as<std::string>()
                         : m_configure_.get_value("service:name", std::string("khlsrv_") + get_context_default_name());
    m_service_display_name = m_configure_.get_value("service:displayName",
                                                    std::string(
                                                        "Khorost Service (") + get_context_default_name() + ")");

    if (vm.count("service")) {
        m_run_as_service = true;

        logger->info("Service = {}", m_service_name);
        SERVICE_TABLE_ENTRY DispatcherTable[] = {
            {(LPSTR)m_service_name.c_str(), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
            {nullptr, nullptr}
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
