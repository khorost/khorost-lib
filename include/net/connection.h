#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <WinSock2.h>
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
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>

#include <deque>

#include "util/autobuffer.h"

#define LOG_CTX_NETWORK			"network"

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
            connection_controller*       m_pController;
            int                         m_ID;
            bufferevent*                m_bev;
            evutil_socket_t             m_fd;
            sockaddr                    m_sa;

            static void stubConnRead(bufferevent* bev_, void* ctx_);
            static void stubConnWrite(bufferevent* bev_, void* ctx_);
            static void stubConnEvent(bufferevent* bev_, short events_, void* ctx_);
        protected:
            data::AutoBufferT<boost::uint8_t>  m_abSocketBuffer;
            size_t      m_nReceiveBytes;
            size_t      m_nSendBytes;

            bool    CompileBufferData();
            virtual size_t data_processing(const boost::uint8_t* buffer, const size_t buffer_size){ return 0; }
        public:
            connection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
            virtual ~connection();

            bool    open_connection();
            bool    close_connection();

            bool    SendData(const boost::uint8_t* pBuffer_, size_t nBufferSize_);
            bool    SendString(const char* pString_, size_t nLength_ = -1);
            bool    SendString(const std::string& rString_);
            bool    SendNumber(unsigned int nNumber_);

            int     GetID() const { return m_ID; }
            connection_controller*    get_controller() { return m_pController; }

            size_t  GetReceiveBytes() const { return m_nReceiveBytes; }
            size_t  GetSendBytes() const { return m_nSendBytes; }

            virtual void    get_client_ip(char* buffer, size_t buffer_size);
        };

        typedef boost::shared_ptr<connection>		connection_ptr;

        class connection_context {
        };

        class connection_controller {
            int						m_nUniqID;
            connection_context*      m_context;
        protected:
            boost::mutex            m_mutex;
            // основной слушающий поток 
            boost::thread*          m_ptListen;
            event_base*             m_pebBaseListen;

            // рабочие потоки
            boost::thread_group*    m_ptgWorkers;
            std::deque<event_base*> m_vWorkersBase;

            static void     stubListenRun(connection_controller* pThis_, int iListenPort_);
            static void     stubWorker(connection_controller* pThis_);
            static void     stubAccept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data);

            int     	GetUniqID(){ return ++m_nUniqID; }

            virtual connection* CreateConnection(connection_controller* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
        public:
            connection_controller(connection_context* pContext_);
            virtual ~connection_controller();

            // «апуск слушащего сокета. ”правление возвращаетс€ сразу
            bool	StartListen(int iListenPort_, int iPollSize_ = 0);
            bool    WaitListen() const;
            bool    Shutdown();

            event_base* GetBaseListen() { return m_pebBaseListen; }

            connection_context*  get_context() const { return m_context; }
            void                set_context(connection_context* context) { m_context = context; }

            connection* add_connection(evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
            bool        remove_connection(connection* pConnection_);
        };
    }
}

#endif // __CONNECTION_H__
