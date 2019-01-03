#pragma once

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  

#include <string>

namespace khorost {
    namespace system {
        class fastfile final {
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
            fastfile(size_ff Granulate = 0);
            virtual ~fastfile();

            // Открывает файл. Если Size_ == 0, тогда файл открывается размером соответствующим действительному размеру файла.
            bool	    open(const std::string& sName_, size_ff Size_ = 0, bool bOnlyRead_ = true);
            // Закрывает файл. Если Size_ != 0, тогда файл закрывается с заданным размером, иначе с m_nRealSize.
            void	    close(size_ff Size_ = 0);

            //	    void*	    GetBlock(size_ff BlockSize_);
            inline void*	get_memory() const { return m_pMemory; }
            size_ff     get_length() const { return m_nFileSize; }
            void	    set_length(size_ff nNewSize_);

            bool	    is_open() const { return m_pMemory != nullptr; }
            bool	    is_read_only() const { return m_bOnlyRead; }

            time_t      get_time_update() const { return m_tUpdate; }
        };
    }
}

