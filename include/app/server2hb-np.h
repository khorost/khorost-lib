#ifndef __SERVER2HB_NP__
#define __SERVER2HB_NP__

#include "net/compactbinary.h"

namespace khorost {
    namespace network {
        class s2bPacket : public cbPacket {
        public:
            static  const sign_cbp    S2B_NP_SIGNATURE = 0x07;

            static  const type_cbp    S2B_NPP_LOGON = 0x0001;
            static  const type_cbp    S2B_NPP_CONNECT = 0x0002;
        };

        class s2bChunk : public cbChunk {
        public:
            static  const type_cbc    S2BC_VERSION = 0x01;
            static  const type_cbc    S2BC_LOGIN = 0x02;
            static  const type_cbc    S2BC_SALT = 0x03;
            static  const type_cbc    S2BC_SALTM = 0x04;
            static  const type_cbc    S2BC_STATUS = 0x05;
            static  const type_cbc    S2BC_HASH = 0x06;
        };

        class s2bConnectClient : public cbChunkIn {
        public:
            // S2BC_LOGIN
            std::string     m_sLogin;

            virtual void    SetValue(id_cbc id_, const std::string& value_) {
                switch (id_) {
                case s2bChunk::S2BC_LOGIN:
                    m_sLogin = value_;
                    break;
                }
            }
        };

        class s2bConnectServer : public cbChunkIn {
        public:
            // S2BC_SALT
            std::string     m_sSalt;
            // S2BC_SALTM
            std::string     m_sSaltM;

            virtual void    SetValue(id_cbc id_, const std::string& value_) {
                switch (id_) {
                case s2bChunk::S2BC_SALT:
                    m_sSalt = value_;
                    break;
                case s2bChunk::S2BC_SALTM:
                    m_sSaltM = value_;
                    break;
                }
            }
        };

        class s2bLogonClient : public cbChunkIn {
        public:
            // S2BC_LOGIN
            std::string     m_sLogin;
            // S2BC_HASH
            std::string     m_sHash;

            virtual void    SetValue(id_cbc id_, const std::string& value_) {
                switch (id_) {
                case s2bChunk::S2BC_LOGIN:
                    m_sLogin = value_;
                    break;
                case s2bChunk::S2BC_HASH:
                    m_sHash = value_;
                    break;
                }
            }
        };

        class s2bLogonServer : public cbChunkIn {
        public:
            // S2BC_STATUS
            boost::uint8_t  m_nStatus;

            virtual void    SetValue8(id_cbc id_, const boost::uint8_t value_) {
                if (id_ == s2bChunk::S2BC_STATUS) {
                    m_nStatus = value_;
                }
            }
        };
    }
}

#endif // __SERVER2HB_NP__
