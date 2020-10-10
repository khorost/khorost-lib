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
            typedef size_t size_ff;
        private:
            size_ff m_granulate_; // Грануляция файла при его увеличении
            size_ff m_file_size_; // Размер реальных данных
            size_ff m_over_file_size_; // Размер "захваченных" данных с учетом грануляции
#ifndef UNIX
            HANDLE m_file_; // идентификатор файла
            HANDLE m_file_map_; // идентификатор проекции
#else
            int	m_file_;	        // идентификатор файла
#endif
            void* m_memory_ = nullptr; // Указатель на начало проекции
            bool m_only_read_; // Файл открыт только для чтения
            time_t m_update_;
        public:
            explicit fastfile(size_ff granulate = 0);
            ~fastfile();

            // Открывает файл. Если Size_ == 0, тогда файл открывается размером соответствующим действительному размеру файла.
            bool open_file(const std::string& file_name, size_ff file_size = 0, bool file_open_mode_only_read = true);
            // Закрывает файл. Если Size_ != 0, тогда файл закрывается с заданным размером, иначе с m_nRealSize.
            void close_file(size_ff size_on_close = 0);

            //	    void*	    GetBlock(size_ff BlockSize_);
            inline void* get_memory() const { return m_memory_; }
            size_ff get_length() const { return m_file_size_; }
            void set_length(size_ff new_size);

            bool is_open() const { return m_memory_ != nullptr; }
            bool is_read_only() const { return m_only_read_; }

            time_t get_time_update() const { return m_update_; }
        };
    }
}
