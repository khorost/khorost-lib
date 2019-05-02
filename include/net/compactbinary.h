#ifndef _COMPACT_BINARY__H_
#define _COMPACT_BINARY__H_

#include <map>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  

#include <boost/cstdint.hpp>

#include "net/connection.h"

#include "util/autobuffer.h"
#include "util/logger.h"

namespace khorost {
    namespace network {

        class cbPacket {
        public:
            typedef boost::uint8_t      sign_cbp;
            typedef boost::uint16_t     size_cbp;
            typedef boost::uint16_t     type_cbp;

#define PACKET_HTON_SIGN(v)        (v)
#define PACKET_HTON_SIZE(v)        htons(v)
#define PACKET_HTON_TYPE(v)        htons(v)

#define PACKET_NTOH_SIGN(v)        (v)
#define PACKET_NTOH_SIZE(v)        ntohs(v)
#define PACKET_NTOH_TYPE(v)        ntohs(v)

            // Вычисляем размер заголовка пакета
            static size_cbp    GetHeaderSize() { return sizeof(sign_cbp) + sizeof(type_cbp) + sizeof(size_cbp); }
        };

        class cbChunk {
        public:
            typedef boost::uint8_t      id_cbc;
            typedef boost::uint16_t     size_cbc;
            typedef boost::uint8_t      type_cbc;

#define CHUNK_HTON_ID(v)          (v)
#define CHUNK_HTON_SIZE(v)        htons(v)
#define CHUNK_HTON_TYPE(v)        (v)

#define CHUNK_NTOH_ID(v)          (v)
#define CHUNK_NTOH_SIZE(v)        ntohs(v)
#define CHUNK_NTOH_TYPE(v)        (v)

            static  const type_cbc    CHUNK_TYPE_BYTE = 0x01;       // 1 байт
            static  const type_cbc    CHUNK_TYPE_SHORT = 0x02;      // 2 байта
            static  const type_cbc    CHUNK_TYPE_INTEGER = 0x03;    // 4 байта
            static  const type_cbc    CHUNK_TYPE_LONG = 0x04;       // 8 байт
            static  const type_cbc    CHUNK_TYPE_STRING = 0x05;     // объект аналогичен CHUNK_TYPE_BINARY
            static  const type_cbc    CHUNK_TYPE_BINARY = 0x07;     // бинарные данные размером до 64К
            static  const type_cbc    CHUNK_TYPE_ARRAY = 0x08;      // массив повторяющихся данных

        public:
            cbChunk() {
            }
            virtual ~cbChunk() {
            }
        };

        class cbChunkIn : public cbChunk {
        public:
            virtual void        SetValue(id_cbc id_, const std::string& value_){}
            virtual void        SetValue(id_cbc id_, const data::AutoBufferT<boost::uint8_t>& value_){}
            virtual void        SetValue8(id_cbc id_, const boost::uint8_t value_){}
            virtual void        SetValue16(id_cbc id_, const boost::uint16_t value_){}
            virtual void        SetValue32(id_cbc id_, const boost::uint32_t value_){}
            virtual void        SetValue64(id_cbc id_, const boost::uint64_t value_){}
            virtual cbChunkIn*  SetArray(id_cbc id_, size_cbc count_) { return NULL; }
            virtual cbChunkIn*  EnterNode(id_cbc id_, size_cbc index_) { return NULL; }
            virtual void        LeaveNode(id_cbc id_, size_cbc index_) {}

            void ParsePacket(const boost::uint8_t* pBuffer_, size_t nBufferSize_);
        };

        class cbChunkOut : public cbChunk {
        protected:
            data::AutoBufferT<boost::uint8_t>   m_abPacket;
        public:
            template<typename T, cbChunk::type_cbc tv_>
            void AppendChunkT(cbChunk::id_cbc id_, T value_) {
                id_ = CHUNK_HTON_ID(id_);
                cbChunk::size_cbc   sizeValue(CHUNK_HTON_SIZE(sizeof(value_)));
                cbChunk::type_cbc   typeValue(CHUNK_HTON_TYPE(tv_));

                m_abPacket.append(reinterpret_cast<const boost::uint8_t*>(&id_), sizeof(id_));
                m_abPacket.append(reinterpret_cast<const boost::uint8_t*>(&typeValue), sizeof(typeValue));
                m_abPacket.append(reinterpret_cast<const boost::uint8_t*>(&sizeValue), sizeof(sizeValue));
                m_abPacket.append(reinterpret_cast<const boost::uint8_t*>(&value_), sizeof(value_));
            }

#define AppendChunkByte(i,v)     AppendChunkT<boost::uint8_t, cbChunk::CHUNK_TYPE_BYTE>(i,v)
#define AppendChunkShort(i,v)    AppendChunkT<boost::uint16_t, cbChunk::CHUNK_TYPE_SHORT>(i,v)
#define AppendChunkInteger(i,v)  AppendChunkT<boost::uint32_t, cbChunk::CHUNK_TYPE_INTEGER>(i,v)

            void    AppendChunkString(cbChunk::id_cbc id_, const std::string& value_);
            void    AppendChunkBuffer(cbChunk::id_cbc id_, const data::AutoBufferT<boost::uint8_t>& value_);

            size_cbc    GetSize() const { return static_cast<size_cbc>(m_abPacket.get_fill_size()); }
            boost::uint8_t* GetBuffer() const { return m_abPacket.get_position(0); }
        };

        template<typename T>
        inline bool CheckBuffer(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
            return nBufferSize_ >= sizeof(T);
        }

        template<typename T>
        inline T ReadBuffer(const boost::uint8_t*& pBuffer_, size_t& nBufferSize_) {
            T v = *(reinterpret_cast<const T*>(pBuffer_));
            pBuffer_ += sizeof(T);
            nBufferSize_ -= sizeof(T);
            return v;
        }

        inline bool CheckBuffer(const boost::uint8_t* pBuffer_, size_t nBufferSize_, cbChunk::size_cbc nSizeValueChunk_) {
            return nBufferSize_ >= nSizeValueChunk_;
        }

        inline void ReadBuffer(const boost::uint8_t*& pBuffer_, size_t& nBufferSize_, cbChunk::size_cbc nSizeValueChunk_, std::string& value_) {
            value_.assign(reinterpret_cast<const char*>(pBuffer_), nSizeValueChunk_);
            pBuffer_ += nSizeValueChunk_;
            nBufferSize_ -= nSizeValueChunk_;
        }

        inline void ReadBuffer(const boost::uint8_t*& pBuffer_, size_t& nBufferSize_, cbChunk::size_cbc nSizeValueChunk_, data::AutoBufferT<boost::uint8_t>& value_) {
            value_.append(pBuffer_, nSizeValueChunk_);
            pBuffer_ += nSizeValueChunk_;
            nBufferSize_ -= nSizeValueChunk_;
        }

        template<typename TD, typename TC, cbPacket::sign_cbp signConst>
        class cbDispatchT {
        protected:
            typedef bool	(TD::* funcProcessCommandCB)(TC&, cbPacket::type_cbp, const boost::uint8_t*, size_t);
            typedef std::map<cbPacket::type_cbp, funcProcessCommandCB>	DictionaryProcessCommandCB;

            DictionaryProcessCommandCB	m_ProcessCommandCB;
            TD*                         m_Context;
        public:
            cbDispatchT(TD* Context_) {
                m_Context = Context_;
            }

            void    RegisterProcessCommandCB(cbPacket::type_cbp type_, funcProcessCommandCB func_) {
                m_ProcessCommandCB[type_] = func_;
            }

            size_t	DoProcessCB(TC& rConnect_, const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
                size_t	nProcessBytes = 0, nHeaderSize = cbPacket::GetHeaderSize();

                while (nBufferSize_ >= nHeaderSize) {
                    cbPacket::sign_cbp      sign = PACKET_NTOH_SIGN(ReadBuffer<cbPacket::sign_cbp>(pBuffer_, nBufferSize_));
                    if (sign == signConst) {
                        cbPacket::size_cbp	sizePacket = PACKET_NTOH_SIZE(ReadBuffer<cbPacket::size_cbp>(pBuffer_, nBufferSize_));
                        cbPacket::type_cbp  typePacket = PACKET_NTOH_TYPE(ReadBuffer<cbPacket::type_cbp>(pBuffer_, nBufferSize_));
                        if (sizePacket <= nBufferSize_) {
                            nProcessBytes += nHeaderSize + sizePacket;

  //                          LOGF(DEBUG, "Process packet type = 0x%04X size = %d bytes", typePacket, sizePacket);

                            auto	itDPCCB = m_ProcessCommandCB.find(typePacket);
                            if (itDPCCB != m_ProcessCommandCB.end()) {
                                funcProcessCommandCB pf = itDPCCB->second;
                                (m_Context->*pf)(rConnect_, typePacket, pBuffer_, sizePacket);

                                pBuffer_ += sizePacket;
                                nBufferSize_ -= sizePacket;
                            } else {
                                // неизвестная команда
                                break;
                            }
                        } else {
                            // пока еще мало данных
                            break;
                        }
                    } else {
                        // неправильный формат
//                        LOGF(WARNING, "Wrong signature - %x", sign);
                        break;
                    }
                }

                return nProcessBytes;
            }

        };

        template<cbPacket::sign_cbp signConst>
        void SendChunkInPacket(network::connection& connect, network::cbPacket::type_cbp tp_, const network::cbChunkOut& ch_) {
            cbPacket::sign_cbp  signPacket(PACKET_HTON_SIGN(signConst));
            cbPacket::size_cbp  packetLength(PACKET_HTON_SIZE(ch_.GetSize()));
            tp_ = PACKET_HTON_TYPE(tp_);

            connect.send_data(reinterpret_cast<boost::uint8_t*>(&signPacket), sizeof(signPacket));
            connect.send_data(reinterpret_cast<boost::uint8_t*>(&packetLength), sizeof(packetLength));
            connect.send_data(reinterpret_cast<boost::uint8_t*>(&tp_), sizeof(tp_));
            connect.send_data(reinterpret_cast<boost::uint8_t*>(ch_.GetBuffer()), ch_.GetSize());
        }
    }
}

#endif  // _COMPACT_BINARY__H_
