#ifndef _AUTOBUFFER__H_
#define _AUTOBUFFER__H_

namespace khorost {
    namespace data {
        template <typename T, size_t nDefaultBufferGranulate = 0x400>
        class AutoBufferT{
            mutable T*			m_pBuffer;			// буфер с данными
            mutable size_t	    m_nFullSize;		// полный размер буфера
            mutable size_t	    m_nReadyPosition;	// позиция в буфере куда можно записывать данные
            bool				m_bAutoFree;		// при "уничтожении" объекта освободить занимаюмую им память
        public:
            // Заполнение массива данными из внешнего источника
            AutoBufferT(const T* pBuffer_, size_t nBufferSize_) :
                m_pBuffer(pBuffer_),
                m_nFullSize(nBufferSize_),
                m_nReadyPosition(nBufferSize_),
                m_bAutoFree(false)
            {
            }
            // Создание массива с автоматическим освобождением
            AutoBufferT() :
                m_pBuffer(NULL),
                m_nFullSize(0),
                m_nReadyPosition(0),
                m_bAutoFree(true)
            {
            }

            virtual ~AutoBufferT(){
                if (m_bAutoFree) {
                    free(m_pBuffer);
                }

                m_pBuffer = NULL;
                m_nFullSize = m_nReadyPosition = 0;
            }

            //	**********************************************************************
            // Кол-во элементов буффера
            size_t  GetFullSize() const { return m_nFullSize; }
            // Размер заполненной области буффера
            size_t  GetFillSize() const { return m_nReadyPosition; }
            // Размер свободной области, доступной для записи
            size_t  GetFreeSize() const { return m_nFullSize - m_nReadyPosition; }
            //	**********************************************************************
            T*  GetHead() const { return m_pBuffer; }
            T*  GetPosition(size_t nPosition_ = 0) const { return m_pBuffer + nPosition_; }
            T*  GetFreePosition() { return m_pBuffer + m_nReadyPosition; }
            //	**********************************************************************
            T   GetElement(size_t nPosition_){ return m_pBuffer[nPosition_]; }
            T   GetLastElement(){ return m_pBuffer[m_nReadyPosition - 1]; }
            T&  operator[](size_t nPosition_) { return m_pBuffer[nPosition_]; }
            //	**********************************************************************
            void FlushFreeSize(){ m_nReadyPosition = 0; }

            size_t  Find(size_t nFrom_, const T* pMatch_, size_t nMatchSize) {
                auto pMax = m_nReadyPosition - nMatchSize;
                for (auto pCheck = nFrom_; pCheck <= pMax; ++pCheck) {
                    if (memcmp(m_pBuffer + pCheck, pMatch_, sizeof(T)*nMatchSize) == 0) {
                        return pCheck;
                    }
                }
                return std::string::npos;
            }

            size_t Compare(size_t nFrom_, const T* pMatch_, size_t nMatchSize) {
                if ((nFrom_ + nMatchSize) > m_nReadyPosition) {
                    return -1;
                } else {
                    return memcmp(m_pBuffer + nFrom_, pMatch_, sizeof(T)*nMatchSize);
                }
            }

            size_t DecrementFreeSize(size_t nSize_){
                if ((m_nReadyPosition + nSize_) < m_nFullSize)
                    m_nReadyPosition += nSize_;
                else
                    m_nReadyPosition = m_nFullSize;
                return m_nReadyPosition;
            }

            size_t IncrementFreeSize(size_t nSize_){
                if (m_nReadyPosition >= nSize_)
                    m_nReadyPosition -= nSize_;
                else
                    m_nReadyPosition = 0;
                return m_nReadyPosition;
            }

            size_t Append(const T* pBuffer_, size_t nCount_, size_t nBufferGranulate_ = nDefaultBufferGranulate){
                if (nCount_ > GetFreeSize()) {
                    CheckSize(GetFillSize() + nCount_, nBufferGranulate_);
                }

                memcpy(GetFreePosition(), pBuffer_, nCount_*sizeof(T));

                m_nReadyPosition += nCount_;
                return m_nReadyPosition;
            }

            void CheckSize(size_t nDemandSize_, size_t nBufferGranulate_ = nDefaultBufferGranulate){
                if (nDemandSize_ == 0)
                    nDemandSize_ = m_nReadyPosition + nBufferGranulate_;

                if (nDemandSize_ > m_nFullSize){
                    m_nFullSize = (nDemandSize_ / nBufferGranulate_ + 1)*nBufferGranulate_;

                    m_pBuffer = (T*)realloc(m_pBuffer, m_nFullSize*sizeof(T));
                    //	буффер "раздувается" по мере приема необработанной информации
                    //	имеет смысл сделать возможность уменьшения рабочего объема по мере
                    //	использования информации из буфера
                }
            }

            void CutFromHead(size_t nCountByte_){
                // TODO вырезание вначале сделать только для случая когда в конце не хватает данных.
                // Тогда при частой предобработке данных слева может ничего не придется реально двигать
                if (nCountByte_ == 0 || nCountByte_ > m_nReadyPosition)
                    return;

                if (nCountByte_ != m_nReadyPosition) {
                    memmove(m_pBuffer, m_pBuffer + nCountByte_, (m_nReadyPosition - nCountByte_)*sizeof(T));
                }

                m_nReadyPosition -= nCountByte_;
            }

            bool Replace(const T* pMatchBuffer_, size_t nMatchCount_
                , const T* pReplaceBuffer_, size_t nReplaceCount_, bool bSingle_ = true) {
                if (m_nReadyPosition < nMatchCount_)
                    return false;

                size_t  nMatchBytes = nMatchCount_*sizeof(T);
                size_t  nReplaceBytes = nReplaceCount_*sizeof(T);
                size_t	nMaxPos = m_nReadyPosition - nMatchCount_;
                bool	bResult = false;

                size_t  nDeltaExpand = 0, nDeltaSub = 0;

                if (nReplaceCount_ != nMatchCount_) {
                    if (nReplaceCount_ > nMatchCount_) {
                        nDeltaExpand = nReplaceCount_ - nMatchCount_;
                    } else {
                        nDeltaSub = nMatchCount_ - nReplaceCount_;
                    }
                }

                for (size_t km = 0; km <= nMaxPos;){
                    if (memcmp(m_pBuffer + km, pMatchBuffer_, nMatchBytes) == 0){
                        if (nDeltaExpand != 0) {
                            CheckSize(m_nReadyPosition + nDeltaExpand);
                        }
                        // сдвигаем хвост
                        if (nDeltaExpand != 0 || nDeltaSub != 0) {
                            memmove(m_pBuffer + km + nReplaceCount_, m_pBuffer + km + nMatchCount_, (m_nReadyPosition - (km + nMatchCount_))*sizeof(T));
                        }
                        memcpy(m_pBuffer + km, pReplaceBuffer_, nReplaceBytes);

                        if (nDeltaExpand) {
                            m_nReadyPosition += nDeltaExpand;
                            nMaxPos += nDeltaExpand;
                            km += nReplaceCount_;
                        } else if (nDeltaSub) {
                            m_nReadyPosition -= nDeltaSub;
                            nMaxPos -= nDeltaSub;
                            km += nMatchCount_;
                        } else {
                            km += nReplaceCount_;
                        }

                        bResult = true;
                        if (bSingle_)
                            break;
                    } else {
                        ++km;
                    }
                }

                return bResult;
            }
        };

        template <typename T, typename S, S SI>
        class AutoBufferChunkT{
            AutoBufferT<T>&         m_rabParent;
            S                       m_nReference;
        public:
            AutoBufferChunkT(AutoBufferT<T>& rabParent_) :
                m_rabParent(rabParent_)
                , m_nReference(SI)
            {
            }

            const T*    GetChunk() const {
                return IsValid() ? m_rabParent.GetPosition(m_nReference) : NULL;
            }

            void    Reset() { m_nReference = SI; }
            void    SetReference(size_t nReference_) { m_nReference = nReference_; }
            S       GetReference() const { return m_nReference; }

            bool    IsValid() const { return m_nReference != SI; }
        };

        typedef AutoBufferT<char>		AutoBufferChar;
        typedef AutoBufferT<wchar_t>	AutoBufferWideChar;

        typedef AutoBufferChunkT<char, size_t, -1>  AutoBufferChunkChar;
    }
}

#endif  // _AUTOBUFFER__H_
