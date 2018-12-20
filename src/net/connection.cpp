#include "net/connection.h"

#include "util/logger.h"

#ifdef WIN32
#pragma comment(lib,"event.lib")
#pragma comment(lib,"event_core.lib")
#pragma comment(lib,"event_extra.lib")
#pragma comment(lib,"wsock32.lib")
#pragma comment(lib,"Ws2_32.lib")
#endif	// WIN32

using namespace khorost::network;

connection::connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    m_pController = pThis_;
    m_ID = ID_;
    m_fd = fd_;
    memcpy(&m_sa, sa_, sizeof(m_sa));
    m_bev = nullptr;

    m_nReceiveBytes = m_nSendBytes = 0;
}

connection::~connection(){
    if (m_bev!= nullptr) {
        bufferevent_free(m_bev);
    }
}

bool connection::SendData(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
    m_nSendBytes += nBufferSize_;
    bufferevent_write(m_bev, pBuffer_, nBufferSize_);
    return true;
}

bool connection::SendString(const std::string& rString_) {
    return SendData(reinterpret_cast<const boost::uint8_t*>(rString_.c_str()), rString_.size());
}

bool connection::SendString(const char* pString_, size_t nLength_) {
    if (nLength_==-1) {
        nLength_ = strlen(pString_);
    }
    return SendData(reinterpret_cast<const boost::uint8_t*>(pString_), nLength_);
}

bool connection::SendNumber(unsigned int nNumber_) {
    char    st[25];
    sprintf(st, "%d", nNumber_);
    return SendString(st);
}

bool connection::CompileBufferData() {
    size_t		s, nProcessedBytes = 0;

    for( s = 0; m_abSocketBuffer.GetFillSize() > s; s += nProcessedBytes ) {
        nProcessedBytes = data_processing(m_abSocketBuffer.GetPosition(s), m_abSocketBuffer.GetFillSize() - s);
        if (nProcessedBytes==0)
            break;
    }
    m_abSocketBuffer.CutFromHead(s);
    return true;
}

void connection::stubConnWrite(bufferevent* bev_, void* ctx_) {
    connection* pThis = static_cast<connection*>(ctx_);
	struct evbuffer *output = bufferevent_get_output(bev_);

    if (evbuffer_get_length(output) == 0) {
		bufferevent_free(bev_);
        pThis->m_bev = nullptr;

        connection_controller* cc = pThis->get_controller();
        if (cc!= nullptr){
            cc->remove_connection(pThis);
        }

        delete pThis;
	}
}

void connection::stubConnRead(bufferevent* bev_, void* ctx_){
    connection* pThis = static_cast<connection*>(ctx_);
    /* This callback is invoked when there is data to read on bev. */
    struct evbuffer* input = bufferevent_get_input(bev_);

    size_t len = evbuffer_get_length(input);
    if (len!=0) {
        pThis->m_abSocketBuffer.CheckSize(len);
        len = bufferevent_read(bev_, static_cast<void*>(pThis->m_abSocketBuffer.GetFreePosition()), len);
        pThis->m_abSocketBuffer.DecrementFreeSize(len);
        pThis->m_nReceiveBytes += len;
        pThis->CompileBufferData();
    }
}

void connection::stubConnEvent(bufferevent* bev_, short events_, void* ctx_){
    connection* pThis = static_cast<connection*>(ctx_);
	if (events_ & BEV_EVENT_EOF) {
		LOG(DEBUG) << "Connection closed.";
	} else if (events_ & BEV_EVENT_ERROR) {
		LOG(WARNING) << "Got an error on the connection: " << strerror(errno);	/*XXX win32*/
	} 

    connection_controller* cc = pThis->get_controller();
    if (cc!= nullptr){
        cc->remove_connection(pThis);
    }

    delete pThis;
}

bool connection::open_connection(){
    event_base* base = m_pController->GetBaseListen();

    m_bev = bufferevent_socket_new(base, m_fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE );
    bufferevent_setcb(m_bev, stubConnRead, nullptr, stubConnEvent, this);
    bufferevent_enable(m_bev, EV_READ|EV_WRITE);

    return true;
}

bool connection::close_connection() {
    struct evbuffer *output = bufferevent_get_output(m_bev);

    bufferevent_setwatermark(m_bev, EV_WRITE,  evbuffer_get_length(output), 0);
	if (evbuffer_get_length(output)) {
        LOG(DEBUG) << "Flush & Close connection";
        /* We still have to flush data from the other
		 * side, but when that's done, close the other
		 * side. */
        bufferevent_setcb(m_bev, nullptr, connection::stubConnWrite, connection::stubConnEvent, this);
	    bufferevent_disable(m_bev, EV_READ);
    } else {
        LOG(DEBUG) << "Close connection";
	    /* We have nothing left to say to the other
	    * side; close it. */
		bufferevent_free(m_bev);
        m_bev = nullptr;

        // TODO необходима разрегистрация соединения
    }

    return true;
}

connection_controller::connection_controller(connection_context* pContext_){
    m_nUniqID = 0;
    m_context = pContext_;
    m_ptListen = nullptr;
    m_ptgWorkers = nullptr;
}

connection_controller::~connection_controller(){
    delete m_ptListen;
    delete m_ptgWorkers;
}

connection* connection_controller::CreateConnection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    return new connection(pThis_, ID_, fd_, sa_, socklen_);
}

bool connection_controller::Shutdown(){
    LOG(INFO) << "Shutdown listner";
    event_base_loopexit(m_pebBaseListen, nullptr);

    LOG(INFO) << "Shutdown workers";
    for (std::deque<event_base*>::const_iterator cit=m_vWorkersBase.begin();cit!=m_vWorkersBase.end();++cit) {
        event_base_loopbreak(*cit);
    }
    m_vWorkersBase.clear();

    LOG(INFO) << "Shutdown complete";
    return true;
}

bool connection_controller::WaitListen() const {
    m_ptListen->join();
    m_ptgWorkers->join_all();

    return true;
}

bool connection_controller::remove_connection(connection* pConnection_){
    boost::mutex::scoped_lock lock(m_mutex);

    LOGF(DEBUG
        , "Close connect #%d. Receive %zu bytes, send %zu bytes"
        , pConnection_->GetID(), pConnection_->GetReceiveBytes(), pConnection_->GetSendBytes());

    return true;
}
// Так делать опасно, осуществляется вызов функции из libevent\util-internal.h
extern "C" const char * evutil_format_sockaddr_port_(const struct sockaddr *sa, char *out, size_t outlen);

connection* connection_controller::add_connection(evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    boost::mutex::scoped_lock lock(m_mutex);

    connection* pConnection = CreateConnection(this, GetUniqID(), fd_, sa_, socklen_);

    char    buf[1024];
    LOGF(DEBUG
        , "Detect incoming connect #%d from %s. Accepted on socket #%X"
        , pConnection->GetID(), evutil_format_sockaddr_port_(sa_, buf, sizeof(buf)), fd_);

    pConnection->open_connection();

    return pConnection;
}

void connection::get_client_ip(char* buffer, size_t buffer_size) {
    evutil_format_sockaddr_port_(&m_sa, buffer, buffer_size);

    for (size_t k = 0; k < buffer_size && buffer[k] != '\0'; ++k) {
        if (buffer[k] == ':') {
            buffer[k] = '\0';
            break;
        }
    }
}

#ifndef WIN32
static void stubSignal(
                      evutil_socket_t sig
                      , short events
                      , void *user_data)
{
    connection_controller* pThis = static_cast<connection_controller*>(user_data);

    printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");
    pThis->Shutdown();
}
#endif

void connection_controller::stubAccept(
                        struct evconnlistener *listener
                        , evutil_socket_t fd
                        , struct sockaddr *sa
                        , int socklen
                        , void *user_data)
{
    connection_controller* pThis = static_cast<connection_controller*>(user_data);
    pThis->add_connection(fd,sa,socklen);
}

static void stubAcceptError(struct evconnlistener *listener, void *ctx) {
    connection_controller* pThis = static_cast<connection_controller*>(ctx);
    struct event_base *base = evconnlistener_get_base(listener);
    const int err = EVUTIL_SOCKET_ERROR();
	LOGF(WARNING, "Got an error %d (%s) on the listener. "
        "Shutting down.\n", err, evutil_socket_error_to_string(err));

    pThis->Shutdown();
}

void connection_controller::stubListenRun(connection_controller* pThis_, int iListenPort_){
    event_config*           cfg = event_config_new();
    evconnlistener*         pelListener;
    sockaddr_in             sin;

    LOG(INFO) << "Start listen";

#ifdef WIN32
    evthread_use_windows_threads();
// включать этот режим для использования с сокетами закрывающимися со стороны сервера нельзя. теряется информация
//    event_config_set_flag(cfg,EVENT_BASE_FLAG_STARTUP_IOCP);
    event_config_set_num_cpus_hint(cfg,4);
#else
    evthread_use_pthreads();
#endif

    pThis_->m_pebBaseListen = event_base_new_with_config(cfg);
    LOG(DEBUG) << "LibEvent Method " << event_base_get_method(pThis_->m_pebBaseListen);

    if (!pThis_->m_pebBaseListen) {
        // fprintf(stderr, "Could not initialize libevent!\n");
        return;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    /* Listen on 0.0.0.0 */
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(iListenPort_);

    pelListener = evconnlistener_new_bind(
        pThis_->m_pebBaseListen
        , stubAccept
        , static_cast<void*>(pThis_)
        , LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE
        , -1
        , reinterpret_cast<struct sockaddr*>(&sin)
        , sizeof(sin));

    if (!pelListener) {
        // fprintf(stderr, "Could not create a listener!\n");
        return;
    }

    evconnlistener_set_error_cb(pelListener, stubAcceptError);

#ifndef WIN32
    event*                  peSignalEvent = evsignal_new(
        pThis_->m_pebBaseListen
        , SIGINT
        , stubSignal
        , (void *)pThis_);

    if (!peSignalEvent || event_add(peSignalEvent, NULL)<0) {
        // fprintf(stderr, "Could not create/add a signal event!\n");
        return;
    }
#endif
    // цикл ожидания и обработки событий
    event_base_dispatch(pThis_->m_pebBaseListen);

    evconnlistener_free(pelListener);
#ifndef WIN32
    event_free(peSignalEvent);
#endif
    event_base_free(pThis_->m_pebBaseListen);
    event_config_free(cfg);

    LOG(INFO) << "Listen stoped";
}

void connection_controller::stubWorker(connection_controller* pThis_){
    LOG(INFO) << "Start worker";
    event_base* base = event_base_new();
    pThis_->m_vWorkersBase.push_back(base);

    // цикл ожидания и обработки событий
    event_base_dispatch(base);

    event_base_free(base);
    LOG(INFO) << "Stop worker";
}

bool connection_controller::StartListen(int iListenPort_, int iPollSize_){
    LOG(INFO) << "Start listen on port " << iListenPort_;
    m_ptgWorkers = new boost::thread_group();
    for (int k=0; k<iPollSize_; ++k) {
//        m_ptgWorkers->create_thread(boost::bind(&stubWorker,this));
    }

    m_ptListen = new boost::thread(boost::bind(&stubListenRun,this, iListenPort_));

    LOG(INFO) << "Starting listen";
    return true;
}

