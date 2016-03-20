#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#ifndef WIN32
/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
#include <unistd.h>
#else
#include <winsock2.h>
#endif  // Win32
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
    namespace Network {
        inline bool Init(){
#ifdef WIN32
            WSADATA wsa_data;
            WSAStartup(0x0201, &wsa_data);
#endif
            return true;
        }

        inline bool Destroy(){
            return true;
        }

        class ConnectionController;

        class Connection {
            ConnectionController*       m_pController;
            int                         m_ID;
            bufferevent*                m_bev;
            evutil_socket_t             m_fd;
            sockaddr                    m_sa;

            static void stubConnRead(bufferevent* bev_, void* ctx_);
            static void stubConnWrite(bufferevent* bev_, void* ctx_);
            static void stubConnEvent(bufferevent* bev_, short events_, void* ctx_);
        protected:
            Data::AutoBufferT<boost::uint8_t>  m_abSocketBuffer;
            size_t      m_nReceiveBytes;
            size_t      m_nSendBytes;

            bool    CompileBufferData();
            virtual size_t DataProcessing(const boost::uint8_t* pBuffer_, size_t nBufferSize_){ return 0; }
        public:
            Connection(ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
            virtual ~Connection();

            bool    OpenConnection();
            bool    CloseConnection();

            bool    SendData(const boost::uint8_t* pBuffer_, size_t nBufferSize_);
            bool    SendString(const char* pString_, size_t nLength_ = -1);
            bool    SendString(const std::string& rString_);
            bool    SendNumber(unsigned int nNumber_);

            int     GetID() const { return m_ID; }
            ConnectionController*    GetController() { return m_pController; }

            size_t  GetReceiveBytes() const { return m_nReceiveBytes; }
            size_t  GetSendBytes() const { return m_nSendBytes; }

            virtual void    GetClientIP(char* pBuffer_, size_t nBufferSize_);
        };

        typedef boost::shared_ptr<Connection>		ConnectionPtr;

        class ConnectionController {
            int						m_nUniqID;
            void*                   m_pContext;
        protected:
            boost::mutex            m_mutex;
            // основной слушающий поток 
            boost::thread*          m_ptListen;
            event_base*             m_pebBaseListen;

            // рабочие потоки
            boost::thread_group*    m_ptgWorkers;
            std::deque<event_base*> m_vWorkersBase;

            static void     stubListenRun(ConnectionController* pThis_, int iListenPort_);
            static void     stubWorker(ConnectionController* pThis_);
            static void     stubAccept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data);

            int     	GetUniqID(){ return ++m_nUniqID; }

            virtual Connection* CreateConnection(ConnectionController* pThis_, int ID_, evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
        public:
            ConnectionController(void* pContext);
            virtual ~ConnectionController();

            // «апуск слушащего сокета. ”правление возвращаетс€ сразу
            bool	StartListen(int iListenPort_, int iPollSize_ = 0);
            bool    WaitListen();
            bool    Shutdown();

            event_base* GetBaseListen() { return m_pebBaseListen; }
            void*       GetContext() { return m_pContext; }
            void        SetContext(void* pContext_) { m_pContext = pContext_; }

            Connection* AddConnection(evutil_socket_t fd_, struct sockaddr* sa_, int socklen_);
            bool        RemoveConnection(Connection* pConnection_);
        };
    }
}

#endif // __CONNECTION_H__
