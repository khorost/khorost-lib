#ifndef _FASTFILE__H_
#define _FASTFILE__H_

#ifndef WIN32
/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
#include <unistd.h>
#else
 #include <windows.h>
#endif  // Win32

#include <string>

namespace khorost {
    namespace System {
        class FastFile {
        public:
            typedef size_t  size_ff;
        private:
            size_ff	    m_nGranulate;       // Грануляция файла при его увеличении
            size_ff	    m_nFileSize;        // Размер реальных данных
            size_ff     m_nOverFileSize;    // Размер "захваченных" данных с учетом грануляции
#ifndef UNIX
            HANDLE		m_hFile;            // идентификатор файла
            HANDLE		m_hFileMap;         // идентификатор проекции
#else
            int			m_hFile;	        // идентификатор файла
#endif
            void*		m_pMemory;	        // Указатель на начало проекции
            bool		m_bOnlyRead;	    // Файл открыт только для чтения
            time_t      m_tUpdate;
        public:
            FastFile(size_ff Granulate = 0);
            virtual ~FastFile();

            // Открывает файл. Если Size_ == 0, тогда файл открывается размером соответствующим действительному размеру файла.
            bool	    Open(const std::string& sName_, size_ff Size_ = 0, bool bOnlyRead_ = true);
            // Закрывает файл. Если Size_ != 0, тогда файл закрывается с заданным размером, иначе с m_nRealSize.
            void	    Close(size_ff Size_ = 0);

            //	    void*	    GetBlock(size_ff BlockSize_);
            inline void*	GetMemory() { return m_pMemory; }
            size_ff     GetLength() const { return m_nFileSize; }
            void	    SetLength(size_ff nNewSize_);

            bool	    IsOpen() const { return m_pMemory != NULL; }
            bool	    IsReadOnly() const { return m_bOnlyRead; }

            time_t      GetTimeUpdate() const { return m_tUpdate; }
        };
    };
}

#endif // _FASTFILE__H_
