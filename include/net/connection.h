#pragma once

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <WinSock2.h>
# include <ws2tcpip.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  
/* For fcntl */
#include <fcntl.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include <boost/utility.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/cstdint.hpp>

#include <deque>

#include "util/autobuffer.h"
#include <thread>

namespace khorost {
    namespace network {
        inline bool init(){
#ifndef UNIX
            WSADATA wsa_data;
            WSAStartup(0x0201, &wsa_data);
#pragma comment(lib,"Ws2_32.lib")
#endif
            return true;
        }

        inline bool destroy(){
            return true;
        }

        class connection_controller;

        class connection {
            connection_controller* m_controller;
            int m_id;
            bufferevent* m_bev;
            evutil_socket_t m_fd;
            sockaddr m_sa;

            static void stub_conn_read(bufferevent* bev, void* ctx);
            static void stub_conn_write(bufferevent* bev, void* ctx);
            static void stub_conn_event(bufferevent* bev, short events, void* ctx);
        protected:
            data::auto_buffer_t<boost::uint8_t>  m_socket_buffer;
            size_t      m_receive_bytes;
            size_t      m_send_bytes;

            bool    compile_buffer_data();
            virtual size_t data_processing(const boost::uint8_t* buffer, const size_t buffer_size){ return 0; }
        public:
            connection(connection_controller* controller, int id, evutil_socket_t fd, struct sockaddr* sa);
            virtual ~connection();

            bool    open_connection();
            bool    close_connection();

            bool    send_data(const boost::uint8_t* buffer, size_t buffer_size);
            bool    send_string(const char* value, size_t length = -1);
            bool    send_string(const std::string& value);
            bool    send_number(unsigned int number);

            int     get_id() const { return m_id; }
            connection_controller*    get_controller() const { return m_controller; }

            size_t  get_receive_bytes() const { return m_receive_bytes; }
            size_t  get_send_bytes() const { return m_send_bytes; }

            virtual void    get_client_ip(char* buffer, size_t buffer_size);
        };

        typedef std::shared_ptr<connection>		connection_ptr;

        class connection_context {
        };

        class connection_controller {
            int						m_uniq_id_;
            connection_context*      m_context_;
        protected:
            boost::mutex            m_mutex_;
            // основной слушающий поток 
            std::thread*          m_listen_thread_;
            event_base*             m_base_listen_;

            // рабочие потоки
            std::deque<event_base*> m_workers_base_;

            static void     stub_listen_run(connection_controller* controller, unsigned short listen_port);
            static void     stub_worker(connection_controller* pThis_);
            static void     stub_accept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *ctx);

            int     	get_uniq_id(){ return ++m_uniq_id_; }

            virtual connection* create_connection(connection_controller* controller, int id, evutil_socket_t fd, struct sockaddr* sa);
        public:
            connection_controller(connection_context* context);
            virtual ~connection_controller();

            // Запуск слушащего сокета. Управление возвращается сразу
            bool	start_listen(unsigned short listen_port);
            bool    wait_listen() const;
            bool    shutdown();

            event_base* get_base_listen() const { return m_base_listen_; }

            connection_context*  get_context() const { return m_context_; }
            void                set_context(connection_context* context) { m_context_ = context; }

            connection* add_connection(evutil_socket_t fd, struct sockaddr* sa);
            bool        remove_connection(connection* connect);
        };
    }
}

