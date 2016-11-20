#include "app/server2hb.h"

#include <openssl/md5.h>
#include <boost/algorithm/hex.hpp>

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
#endif  // UNIX


Server2HB::Server2HB() :
    m_Connections(this)
    , m_dbBase(m_DB)
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

bool Server2HB::Prepare(int argc_, char* argv_[]) {
    g_pS2HB = this;
#ifdef UNIX
    setlocale(LC_ALL, "");

    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, UNIXSignal);
    signal(SIGINT, UNIXSignal);
#else
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)&WinSignal, TRUE);
#endif
    khorost::Network::Init();

    m_dictActionS2H.insert(std::pair<std::string, funcActionS2H>(S2H_PARAM_ACTION_AUTH, &Server2HB::ActionAuth));

/*    khorost::System::FastFile    ff;

    if (ff.Open("autoexec.json", -1, true)) {
        Json::Value root;
        Json::Reader reader;

        char* pAUJ = reinterpret_cast<char*>(ff.GetMemory());
        if (reader.parse(pAUJ, pAUJ + ff.GetLength(), root)) {
            Json::Value jsCreateUser = root["Create"]["User"];
            for (Json::Value::ArrayIndex k = 0; k < jsCreateUser.size(); ++k) {
                Json::Value jsUser = jsCreateUser[k];

                if (!m_dbStorage.IsUserExist(jsUser["Login"].asString())) {
                    m_dbStorage.CreateUser(jsUser);
                }
            }
        }

        ff.Close();

        //        ReparseExportFiles();
    }
    */
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
    LOGF(DEBUG, "[HTTP_PROCESS]");

    HTTPTextProtocolHeader&     rHTTP = rConnect_.GetHTTP();
    SessionPtr                  sp = ProcessingSession(rConnect_, rHTTP);
    S2HSession*                 s2hSession = reinterpret_cast<S2HSession*>(sp.get());
    const char*                 pQueryURI = rHTTP.GetQueryURI();

    LOGF(DEBUG, "[HTTP_PROCESS] URI='%s'", pQueryURI);

    if (strncmp(pQueryURI, m_sURLPrefixAction.c_str(), m_sURLPrefixAction.size())==0) {
        const char* pQueryAction = rHTTP.GetParameter(S2H_PARAM_ACTION);
        if (pQueryAction != NULL) {
            if (!ProcessHTTPCommand(pQueryAction, s2hSession, rConnect_, rHTTP)) {
                return true;
            }
        }

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
    return hft.SendFile(sQueryURI_, rConnect_, rHTTP_, m_strDocRoot);
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
        if ((ptNow - ptSessionUpdate).minutes() > 10) {
            ptSessionUpdate = ptNow;
            pThis_->TimerSessionUpdate();
        }
        if ((ptNow - ptSessionIPUpdate).hours() > 1) {
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
    const char* pQuerySession = rHTTP_.GetCookie(S2H_SESSION);
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

    rHTTP_.SetCookie(S2H_SESSION, sp->GetSessionID(), sp->GetExpired(), rHTTP_.GetHost());

    LOGF(DEBUG, S2H_SESSION " = '%s' ClientIP = '%s' ConnectID = #%d InS = '%s' "
        , sp->GetSessionID().c_str(), sIP, rConnect_.GetID()
        , pQuerySession != NULL ? (strcmp(sp->GetSessionID().c_str(), pQuerySession) == 0 ? "+" : pQuerySession) : "-"
    );
    return sp;
}

bool Server2HB::ActionAuth(Network::Connection& rConnect_, S2HSession* pSession_, HTTPTextProtocolHeader& rHTTP_) {
    using namespace boost::posix_time;

    LOGF(DEBUG, "[ActionAuth]");
    Json::Value jvRoot;

    const char* pActionParam = rHTTP_.GetParameter(S2H_PARAM_ACTION_PARAM);
    if (pActionParam != NULL) {
        LOGF(DEBUG, "[ActionAuth] ActionParam = '%s'", pActionParam);
        std::string sLogin, sNickname, sPwHash, sSalt;
        int nUserID;

        m_DB.CheckConnect();

        const char* pQueryQ = rHTTP_.GetParameter(S2H_PARAM_QUESTION);
        if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_PRE) == 0) {
            if (m_dbBase.GetUserInfo(pQueryQ != NULL ? pQueryQ : "", nUserID, sNickname, sPwHash, sSalt)) {
                jvRoot[S2H_JSON_SALT] = sSalt;
            } else {
                // не найден пользователь. но так в принципе наверно не стоит 
                LOGF(INFO, "User not found");
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
                    LOGF(INFO, "Password not correct");
                }
            } else {
                // не найден пользователь
                LOGF(INFO, "User not found");
            }
        } else if (strcmp(pActionParam, S2H_PARAM_ACTION_AUTH_RESET) == 0) {
            pSession_->Reset();
            pSession_->SetExpired(second_clock::universal_time());
            rHTTP_.SetCookie(S2H_SESSION, pSession_->GetSessionID(), pSession_->GetExpired(), rHTTP_.GetHost());
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

bool Server2HB::JSON_PingPong(Network::HTTPTextProtocolHeader& rHTTP_, Json::Value& jvRoot_) {
    bool bExist = false;
    const char* pPing = rHTTP_.GetParameter(S2H_PARAM_ACTION_PING, &bExist);
    if (bExist) {
        jvRoot_[S2H_JSON_PONG] = pPing;
    }
    return true;
}

std::string Server2HB::JSONWrite(const Json::Value& jvRoot_, bool bStyled_) {
    if (bStyled_) {
        return Json::StyledWriter().write(jvRoot_);
    } else {
        return Json::FastWriter().write(jvRoot_);
    }
}

bool Server2HB::JSON_FillAuth(Network::S2HSession* pSession_, bool bFullInfo_, Json::Value& jvRoot_) {
    jvRoot_[S2H_JSON_AUTH] = pSession_->IsAuthenticate();
    if (bFullInfo_ && pSession_->IsAuthenticate()) {
        jvRoot_[S2H_JSON_NICKNAME] = pSession_->GetNickname();
        Json::Value jvRoles;
        pSession_->FillRoles(jvRoles);
        jvRoot_[S2H_JSON_ROLES] = jvRoles;
    }

    return true;
}

