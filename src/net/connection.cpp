#include "net/connection.h"

#include "util/logger.h"
#include "app/khl-define.h"

#ifdef WIN32
#pragma comment(lib,"event.lib")
#pragma comment(lib,"event_core.lib")
#pragma comment(lib,"event_extra.lib")
#pragma comment(lib,"wsock32.lib")
#pragma comment(lib,"Ws2_32.lib")
#endif	// WIN32

#define LOGGER_PREFIX   "[CONNECTION] "

using namespace khorost::network;

connection::connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    m_controller = pThis_;
    m_id = ID_;
    m_fd = fd_;
    memcpy(&m_sa, sa_, sizeof(m_sa));
    m_bev = nullptr;

    m_receive_bytes = m_send_bytes = 0;
}

connection::~connection(){
    if (m_bev!= nullptr) {
        bufferevent_free(m_bev);
    }
}

bool connection::send_data(const boost::uint8_t* buffer, const size_t buffer_size) {
    m_send_bytes += buffer_size;
    bufferevent_write(m_bev, buffer, buffer_size);
    return true;
}

bool connection::send_string(const std::string& value) {
    return send_data(reinterpret_cast<const boost::uint8_t*>(value.c_str()), value.size());
}

bool connection::send_string(const char* value, size_t length) {
    if (length==-1) {
        length = strlen(value);
    }
    return send_data(reinterpret_cast<const boost::uint8_t*>(value), length);
}

bool connection::send_number(unsigned int number) {
    char    st[25];
    sprintf(st, "%d", number);
    return send_string(st);
}

bool connection::compile_buffer_data() {
    size_t s, processed_bytes = 0;

    for (s = 0; m_socket_buffer.GetFillSize() > s; s += processed_bytes) {
        processed_bytes = data_processing(m_socket_buffer.GetPosition(s), m_socket_buffer.GetFillSize() - s);
        if (processed_bytes == 0)
            break;
    }
    m_socket_buffer.CutFromHead(s);
    return true;
}

void connection::stub_conn_write(bufferevent* bev, void* ctx) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    const auto connect = static_cast<connection*>(ctx);
    const auto output = bufferevent_get_output(bev);

    if (evbuffer_get_length(output) == 0) {
        logger->debug(LOGGER_PREFIX"Connection close on write");

        bufferevent_free(bev);
        connect->m_bev = nullptr;

        auto controller = connect->get_controller();
        if (controller != nullptr) {
            controller->remove_connection(connect);
        }

        delete connect;
    }
}

void connection::stub_conn_read(bufferevent* bev, void* ctx) {
    auto connect = static_cast<connection*>(ctx);
    /* This callback is invoked when there is data to read on bev. */
    const auto input = bufferevent_get_input(bev);
    auto len = evbuffer_get_length(input);

    if (len != 0) {
        connect->m_socket_buffer.check_size(len);
        len = bufferevent_read(bev, static_cast<void*>(connect->m_socket_buffer.get_free_position()), len);
        connect->m_socket_buffer.DecrementFreeSize(len);
        connect->m_receive_bytes += len;

        connect->compile_buffer_data();
    }
}

void connection::stub_conn_event(bufferevent* bev, short events, void* ctx) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    if (events & BEV_EVENT_EOF) {
        logger->debug(LOGGER_PREFIX" Connection closed");
    } else if (events & BEV_EVENT_ERROR) {
        logger->warn(LOGGER_PREFIX"Got an error on the connection: {}", strerror(errno)); /*XXX win32*/
    }

    const auto connect = static_cast<connection*>(ctx);
    auto controller = connect->get_controller();
    if (controller != nullptr) {
        controller->remove_connection(connect);
    }

    delete connect;
}

bool connection::open_connection(){
    event_base* base = m_controller->GetBaseListen();

    m_bev = bufferevent_socket_new(base, m_fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE );
    bufferevent_setcb(m_bev, stub_conn_read, nullptr, stub_conn_event, this);
    bufferevent_enable(m_bev, EV_READ|EV_WRITE);

    return true;
}

bool connection::close_connection() {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    struct evbuffer *output = bufferevent_get_output(m_bev);

    bufferevent_setwatermark(m_bev, EV_WRITE,  evbuffer_get_length(output), 0);
	if (evbuffer_get_length(output)) {
        logger->debug(LOGGER_PREFIX"Flush & Close connection");
        /* We still have to flush data from the other
		 * side, but when that's done, close the other
		 * side. */
        bufferevent_setcb(m_bev, nullptr, connection::stub_conn_write, connection::stub_conn_event, this);
	    bufferevent_disable(m_bev, EV_READ);
    } else {
        logger->debug(LOGGER_PREFIX"Close connection");
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

connection* connection_controller::create_connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    return new connection(pThis_, ID_, fd_, sa_, socklen_);
}

bool connection_controller::shutdown(){
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    logger->info(LOGGER_PREFIX"shutdown listner");
    event_base_loopexit(m_pebBaseListen, nullptr);

    logger->info(LOGGER_PREFIX"shutdown workers");
    for (std::deque<event_base*>::const_iterator cit=m_vWorkersBase.begin();cit!=m_vWorkersBase.end();++cit) {
        event_base_loopbreak(*cit);
    }
    m_vWorkersBase.clear();

    logger->info(LOGGER_PREFIX"shutdown complete");
    return true;
}

bool connection_controller::wait_listen() const {
    m_ptListen->join();
    m_ptgWorkers->join_all();

    return true;
}

bool connection_controller::remove_connection(connection* connect){
    boost::mutex::scoped_lock lock(m_mutex);

    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->debug(LOGGER_PREFIX"Close connect #{:d}. Receive {:d} bytes, send {:d} bytes"
        , connect->get_id(), connect->get_receive_bytes(), connect->get_send_bytes());

    return true;
}
// Так делать опасно, осуществляется вызов функции из libevent\util-internal.h
extern "C" const char * evutil_format_sockaddr_port_(const struct sockaddr *sa, char *out, size_t outlen);

connection* connection_controller::add_connection(evutil_socket_t fd_, struct sockaddr* sa_, int socklen_){
    boost::mutex::scoped_lock lock(m_mutex);

    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    auto pConnection = create_connection(this, GetUniqID(), fd_, sa_, socklen_);

    char    buf[1024];
    logger->debug(LOGGER_PREFIX"Detect incoming connect #{:d} from {}. Accepted on socket #{:X}"
        , pConnection->get_id(), evutil_format_sockaddr_port_(sa_, buf, sizeof(buf)), fd_);

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
    pThis->shutdown();
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
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    auto pThis = static_cast<connection_controller*>(ctx);
    struct event_base *base = evconnlistener_get_base(listener);
    const int err = EVUTIL_SOCKET_ERROR();
	logger->warn(LOGGER_PREFIX"Got an error {:d} ({}) on the listener. Shutting down.", err, evutil_socket_error_to_string(err));

    pThis->shutdown();
}

void connection_controller::stubListenRun(connection_controller* pThis_, int iListenPort_){
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    event_config*           cfg = event_config_new();
    evconnlistener*         pelListener;
    sockaddr_in             sin;

    logger->info(LOGGER_PREFIX"Start listen");

#ifdef WIN32
    evthread_use_windows_threads();
// включать этот режим для использования с сокетами закрывающимися со стороны сервера нельзя. теряется информация
//    event_config_set_flag(cfg,EVENT_BASE_FLAG_STARTUP_IOCP);
    event_config_set_num_cpus_hint(cfg,4);
#else
    evthread_use_pthreads();
#endif

    pThis_->m_pebBaseListen = event_base_new_with_config(cfg);
    logger->debug(LOGGER_PREFIX"LibEvent Method {}" ,event_base_get_method(pThis_->m_pebBaseListen));

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

    logger->info(LOGGER_PREFIX"Listen stopped");
}

void connection_controller::stubWorker(connection_controller* pThis_){
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->info(LOGGER_PREFIX"Start worker");
    event_base* base = event_base_new();
    pThis_->m_vWorkersBase.push_back(base);

    // цикл ожидания и обработки событий
    event_base_dispatch(base);

    event_base_free(base);
    logger->info(LOGGER_PREFIX"Stop worker");
}

bool connection_controller::StartListen(int iListenPort_, int iPollSize_){
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->info(LOGGER_PREFIX"Start listen on port {:d}", iListenPort_);
    m_ptgWorkers = new boost::thread_group();
    for (int k=0; k<iPollSize_; ++k) {
//        m_ptgWorkers->create_thread(boost::bind(&stubWorker,this));
    }

    m_ptListen = new boost::thread(boost::bind(&stubListenRun,this, iListenPort_));

    logger->info(LOGGER_PREFIX"Starting listen");
    return true;
}

