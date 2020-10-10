// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

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

connection::connection(connection_controller* controller, const int id, const evutil_socket_t fd, struct sockaddr* sa) {
    m_controller = controller;
    m_id = id;
    m_fd = fd;
    memcpy(&m_sa, sa, sizeof(m_sa));

    m_bev = nullptr;
    m_receive_bytes = m_send_bytes = 0;
}

connection::~connection() {
    if (m_bev != nullptr) {
        bufferevent_free(m_bev);
        m_bev = nullptr;
    }
}

bool connection::send_data(const uint8_t* buffer, const size_t buffer_size) {
    m_send_bytes += buffer_size;
    bufferevent_write(m_bev, buffer, buffer_size);
    return true;
}

bool connection::send_string(const std::string& value) {
    return send_data(reinterpret_cast<const uint8_t*>(value.c_str()), value.size());
}

bool connection::send_string(const char* value, size_t length) {
    if (length == static_cast<size_t>(-1)) {
        length = strlen(value);
    }
    return send_data(reinterpret_cast<const uint8_t*>(value), length);
}

bool connection::send_number(const unsigned int number) {
    char st[25];
    sprintf(st, "%d", number);
    return send_string(st);
}

bool connection::compile_buffer_data() {
    size_t s, processed_bytes;

    for (s = 0; m_socket_buffer.get_fill_size() > s; s += processed_bytes) {
        processed_bytes = data_processing(m_socket_buffer.get_position(s), m_socket_buffer.get_fill_size() - s);
        if (processed_bytes == 0)
            break;
    }
    m_socket_buffer.cut_from_head(s);
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
        connect->m_socket_buffer.check_size(connect->m_socket_buffer.get_fill_size() + len);
        len = bufferevent_read(bev, static_cast<void*>(connect->m_socket_buffer.get_free_position()), len);
        connect->m_socket_buffer.decrement_free_size(len);
        connect->m_receive_bytes += len;

        connect->compile_buffer_data();
    }
}

void connection::stub_conn_event(bufferevent* bev, const short events, void* ctx) {
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

bool connection::open_connection() {
    const auto base = m_controller->get_base_listen();

    m_bev = bufferevent_socket_new(base, m_fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    bufferevent_setcb(m_bev, stub_conn_read, nullptr, stub_conn_event, this);
    bufferevent_enable(m_bev, EV_READ | EV_WRITE);

    return true;
}

bool connection::close_connection() {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    const auto output = bufferevent_get_output(m_bev);

    bufferevent_setwatermark(m_bev, EV_WRITE, evbuffer_get_length(output), 0);
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

connection_controller::connection_controller(connection_context* context): m_uniq_id_(0), m_context_(context),
                                                                           m_listen_thread_(nullptr),
                                                                           m_base_listen_(nullptr) {
}

connection_controller::~connection_controller() {
    delete m_listen_thread_;
}

connection* connection_controller::create_connection(connection_controller* controller, const int id,
                                                     const evutil_socket_t fd,
                                                     struct sockaddr* sa) {
    return new connection(controller, id, fd, sa);
}

bool connection_controller::shutdown() {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    logger->info(LOGGER_PREFIX"shutdown listner");
    event_base_loopexit(m_base_listen_, nullptr);

    logger->info(LOGGER_PREFIX"shutdown workers");
    for (std::deque<event_base*>::const_iterator cit = m_workers_base_.begin(); cit != m_workers_base_.end(); ++cit) {
        event_base_loopbreak(*cit);
    }
    m_workers_base_.clear();

    logger->info(LOGGER_PREFIX"shutdown complete");
    return true;
}

bool connection_controller::wait_listen() const {
    m_listen_thread_->join();

    return true;
}

bool connection_controller::remove_connection(connection* connect) {
    boost::mutex::scoped_lock lock(m_mutex_);

    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->debug(LOGGER_PREFIX"Close connect #{:d}. Receive {:d} bytes, send {:d} bytes"
                  , connect->get_id(), connect->get_receive_bytes(), connect->get_send_bytes());

    return true;
}

// Так делать опасно, осуществляется вызов функции из libevent\util-internal.h
const char* x_evutil_format_sockaddr_port(const struct sockaddr* sa, char* out, const size_t outlen) {
    char b[128];
    const char* res;
    int port;

    if (sa->sa_family == AF_INET) {
        const auto sin = reinterpret_cast<const sockaddr_in*>(sa);
        res = evutil_inet_ntop(AF_INET, &sin->sin_addr, b, sizeof(b));
        port = ntohs(sin->sin_port);
        if (res) {
            evutil_snprintf(out, outlen, "%s:%d", b, port);
            return out;
        }
    } else if (sa->sa_family == AF_INET6) {
        const auto sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
        res = evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, b, sizeof(b));
        port = ntohs(sin6->sin6_port);
        if (res) {
            evutil_snprintf(out, outlen, "[%s]:%d", b, port);
            return out;
        }
    }

    evutil_snprintf(out, outlen, "<addr with socktype %d>",
                    (int)sa->sa_family);
    return out;
}

connection* connection_controller::add_connection(const evutil_socket_t fd, struct sockaddr* sa) {
    boost::mutex::scoped_lock lock(m_mutex_);

    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    auto conn = create_connection(this, get_uniq_id(), fd, sa);

    char buf[1024];
    logger->debug(LOGGER_PREFIX"Detect incoming connect #{:d} from {}. Accepted on socket #{:X}"
                  , conn->get_id(), x_evutil_format_sockaddr_port(sa, buf, sizeof(buf)), fd);

    conn->open_connection();

    return conn;
}

void connection::get_client_ip(char* buffer, size_t buffer_size) {
    x_evutil_format_sockaddr_port(&m_sa, buffer, buffer_size);

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

void connection_controller::stub_accept(struct evconnlistener*, const evutil_socket_t fd, struct sockaddr* sa, int,
                                        void* ctx) {
    auto controller = static_cast<connection_controller*>(ctx);
    controller->add_connection(fd, sa);
}

static void stub_accept_error(struct evconnlistener* listener, void* ctx) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    auto controller = static_cast<connection_controller*>(ctx);
    const auto err = EVUTIL_SOCKET_ERROR();
    logger->warn(LOGGER_PREFIX"Got an error {:d} ({}) on the listener. Shutting down.", err,
                 evutil_socket_error_to_string(err));

    controller->shutdown();
}

void connection_controller::stub_listen_run(connection_controller* controller, const unsigned short listen_port) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    const auto cfg = event_config_new();
    evconnlistener* listener;
    sockaddr_in sin;

    logger->info(LOGGER_PREFIX"Start listen");

#ifdef WIN32
    evthread_use_windows_threads();
    // включать этот режим для использования с сокетами закрывающимися со стороны сервера нельзя. теряется информация
    //    event_config_set_flag(cfg,EVENT_BASE_FLAG_STARTUP_IOCP);
    event_config_set_num_cpus_hint(cfg, 4);
#else
    evthread_use_pthreads();
#endif

    controller->m_base_listen_ = event_base_new_with_config(cfg);
    if (!controller->m_base_listen_) {
        logger->error(LOGGER_PREFIX"Could not initialize libevent!");
        return;
    }

    logger->debug(LOGGER_PREFIX"LibEvent Method {}", event_base_get_method(controller->m_base_listen_));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    /* Listen on 0.0.0.0 */
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(listen_port);

    listener = evconnlistener_new_bind(
        controller->m_base_listen_
        , stub_accept
        , static_cast<void*>(controller)
        , LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE
        , -1
        , reinterpret_cast<struct sockaddr*>(&sin)
        , sizeof(sin));

    if (!listener) {
        logger->error(LOGGER_PREFIX"Could not create a listener!");
        return;
    }

    evconnlistener_set_error_cb(listener, stub_accept_error);

#ifndef WIN32
    event*                  peSignalEvent = evsignal_new(
        controller->m_base_listen_
        , SIGINT
        , stubSignal
        , (void *)controller);

    if (!peSignalEvent || event_add(peSignalEvent, NULL)<0) {
        logger->error(LOGGER_PREFIX"Could not create/add a signal event!");
        return;
    }
#endif
    // цикл ожидания и обработки событий
    event_base_dispatch(controller->m_base_listen_);

    evconnlistener_free(listener);
#ifndef WIN32
    event_free(peSignalEvent);
#endif
    event_base_free(controller->m_base_listen_);
    event_config_free(cfg);

    logger->info(LOGGER_PREFIX"Listen stopped");
}

void connection_controller::stub_worker(connection_controller* controller) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->info(LOGGER_PREFIX"Start worker");

    const auto base = event_base_new();
    controller->m_workers_base_.push_back(base);

    // цикл ожидания и обработки событий
    event_base_dispatch(base);

    event_base_free(base);
    logger->info(LOGGER_PREFIX"Stop worker");
}

bool connection_controller::start_listen(const unsigned short listen_port) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);
    logger->info(LOGGER_PREFIX"Start listen on port {:d}", listen_port);

    m_listen_thread_ = new std::thread([this, listen_port] { return stub_listen_run(this, listen_port); });

    logger->info(LOGGER_PREFIX"Starting listen");
    return true;
}
