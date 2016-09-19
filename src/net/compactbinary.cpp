#include "net/compactbinary.h"

using namespace khorost::Network;
using namespace khorost::Data;

union uint64x32_t {
    boost::uint64_t	m_ui64;
    struct ui32 {
        boost::uint32_t	m_uiLow;
        boost::uint32_t	m_uiHigh;
    }			m_ui32;
};

void cbChunkIn::ParsePacket(const boost::uint8_t* pBuffer_, size_t nBufferSize_) {
    // Блоки в формате ITLV
    id_cbc      id;
    type_cbc    type;
    size_cbc    length;
    std::string sv;
    Data::AutoBufferT<boost::uint8_t>   ab;

    while (true) {
        if (!CheckBuffer<id_cbc>(pBuffer_, nBufferSize_)) {
            break;
        }
        id = CHUNK_NTOH_ID(ReadBuffer<id_cbc>(pBuffer_, nBufferSize_));

        if (!CheckBuffer<type_cbc>(pBuffer_, nBufferSize_)) {
            break;
        }
        type = CHUNK_NTOH_TYPE(ReadBuffer<type_cbc>(pBuffer_, nBufferSize_));

        if (!CheckBuffer<size_cbc>(pBuffer_, nBufferSize_)) {
            break;
        }
        length = CHUNK_NTOH_SIZE(ReadBuffer<size_cbc>(pBuffer_, nBufferSize_));
        if (length>nBufferSize_) {
            break;  // ошибка. ожидается данных больше чем их представлено
        }

        switch(type){
        case CHUNK_TYPE_BYTE:
            if (CheckBuffer<boost::uint8_t>(pBuffer_, nBufferSize_)) {
                SetValue8(id, ReadBuffer<boost::uint8_t>(pBuffer_, nBufferSize_));
            }
            break;
        case CHUNK_TYPE_SHORT:
            if (CheckBuffer<boost::uint16_t>(pBuffer_, nBufferSize_)) {
                SetValue16(id, ntohs(ReadBuffer<boost::uint16_t>(pBuffer_, nBufferSize_)));
            }
            break;
        case CHUNK_TYPE_INTEGER:
            if (CheckBuffer<boost::uint32_t>(pBuffer_, nBufferSize_)) {
                SetValue32(id, ntohl(ReadBuffer<boost::uint32_t>(pBuffer_, nBufferSize_)));
            }
            break;
        case CHUNK_TYPE_LONG:
            if (CheckBuffer<boost::uint64_t>(pBuffer_, nBufferSize_)) {
                uint64x32_t uiTemp;
                uiTemp.m_ui64 = ReadBuffer<boost::uint64_t>(pBuffer_, nBufferSize_);
                uiTemp.m_ui32.m_uiHigh = ntohl(uiTemp.m_ui32.m_uiHigh);
                uiTemp.m_ui32.m_uiLow = ntohl(uiTemp.m_ui32.m_uiLow);
                SetValue64(id, uiTemp.m_ui64);
            }
            break;
        case CHUNK_TYPE_STRING:
            if (CheckBuffer(pBuffer_, nBufferSize_, length)) {
                ReadBuffer(pBuffer_, nBufferSize_, length, sv);
                SetValue(id, sv);
            }
            break;
        case CHUNK_TYPE_BINARY:
            if (CheckBuffer(pBuffer_, nBufferSize_, length)) {
                ab.FlushFreeSize();
                ReadBuffer(pBuffer_, nBufferSize_, length, ab);
                SetValue(id, ab);
            }
            break;
        case CHUNK_TYPE_ARRAY:{
                const boost::uint8_t* pBuffer = pBuffer_;
                size_t nBufferSize = length;
                size_cbc count;

                if (CheckBuffer<size_cbc>(pBuffer, nBufferSize)) {
                    count = ReadBuffer<size_cbc>(pBuffer, nBufferSize);
                    cbChunkIn*    pArray = SetArray(id, count);
                    if (pArray!=NULL){
                        size_cbc idx = 0;
                        while (true) {
                            if (CheckBuffer<size_cbc>(pBuffer, nBufferSize)) {
                                size_cbc nodeLength = ReadBuffer<size_cbc>(pBuffer, nBufferSize);
                                cbChunkIn*    pNode = pArray->EnterNode(id, idx);
                                if (pNode!=NULL) {
                                    pNode->ParsePacket(pBuffer, nodeLength);
                                    pNode->LeaveNode(id, idx);
                                }
                                pBuffer += nodeLength;
                                nBufferSize -= nodeLength;
                            } else {
                                break;
                            }
                            ++idx;
                        }
                    }
                }
                pBuffer_ += length;
                nBufferSize_ -= length;
            }
            break;
        default:
            // chunk не распознали, переходим к следующему
            pBuffer_ += length;
            nBufferSize_ -= length;
            break;
        }
    }
}

void cbChunkOut::AppendChunkString(cbChunk::id_cbc id_, const std::string& value_) {
    id_ = CHUNK_HTON_ID(id_);
    cbChunk::size_cbc   sizeValue(CHUNK_HTON_SIZE((cbChunk::size_cbc)value_.size()));
    cbChunk::type_cbc   typeValue(CHUNK_HTON_TYPE(cbChunk::CHUNK_TYPE_STRING));

    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&id_), sizeof(id_));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&typeValue), sizeof(typeValue));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&sizeValue), sizeof(sizeValue));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(value_.c_str()), value_.size());
}

void cbChunkOut::AppendChunkBuffer(cbChunk::id_cbc id_, const AutoBufferT<boost::uint8_t>& value_) {
    id_ = CHUNK_HTON_ID(id_);
    cbChunk::size_cbc   sizeValue(CHUNK_HTON_SIZE((cbChunk::size_cbc)value_.GetFillSize()));
    cbChunk::type_cbc   typeValue(CHUNK_HTON_TYPE(cbChunk::CHUNK_TYPE_BINARY));

    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&id_), sizeof(id_));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&typeValue), sizeof(typeValue));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(&sizeValue), sizeof(sizeValue));
    m_abPacket.Append(reinterpret_cast<const boost::uint8_t*>(value_.GetPosition(0)), value_.GetFullSize());
}
