// fast_file.cpp: implementation of the CFastFile class.
//
//////////////////////////////////////////////////////////////////////


#include <stdlib.h>

#include "system/fastfile.h"

#ifdef UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/mman.h>             
 #include <fcntl.h>
 #include <unistd.h>
#else
 #include <stdio.h>
 #include <time.h>
#endif

using namespace khorost::system;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

fastfile::fastfile(size_ff granulate):
	m_granulate_(granulate)
    , m_file_size_(0)
    , m_over_file_size_(0){
#ifndef UNIX
    m_file_ = INVALID_HANDLE_VALUE;
    m_file_map_ = nullptr;

    if (m_granulate_ == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        m_granulate_ = si.dwAllocationGranularity;
    }
#else
    m_file_ = -1;
	if(m_granulate_==0) {
        m_granulate_ = sysconf(_SC_PAGESIZE);
    }
#endif
}

fastfile::~fastfile() {
	close_file();
}

bool fastfile::open_file(const std::string& file_name, const size_ff file_size, const bool file_open_mode_only_read) {
    close_file();

    m_only_read_ = file_open_mode_only_read;

#ifndef UNIX
    if (file_open_mode_only_read && GetFileAttributes(file_name.c_str()) == -1) {
        return false;
    }

    m_file_ = CreateFile(file_name.c_str(), m_only_read_ ? GENERIC_READ : GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_file_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILETIME ft_create, ft_access, ft_write;
    SYSTEMTIME st_utc;

    GetFileTime(m_file_, &ft_create, &ft_access, &ft_write);
    FileTimeToSystemTime(&ft_write, &st_utc);

    tm _tm;

    memset(&_tm, 0, sizeof(tm));

    _tm.tm_year = st_utc.wYear - 1900;
    _tm.tm_mon = st_utc.wMonth - 1;
    _tm.tm_mday = st_utc.wDay;
    _tm.tm_hour = st_utc.wHour;
    _tm.tm_min = st_utc.wMinute;
    _tm.tm_sec = st_utc.wSecond;

    _tm.tm_isdst = -1;

    m_update_ = _mkgmtime(&_tm);

    m_file_size_ = GetFileSize(m_file_, nullptr);
    if (m_file_size_ == 0) {
        m_file_size_ = file_size;
    }

    if (!m_only_read_) {
        m_over_file_size_ = (m_file_size_ / m_granulate_ + 1) * m_granulate_;
    } else {
        m_over_file_size_ = m_file_size_;
    }

    m_file_map_ = CreateFileMapping(m_file_, nullptr, m_only_read_ ? PAGE_READONLY : PAGE_READWRITE, 0, m_over_file_size_, nullptr);
    if (m_file_map_ == nullptr) {
        return false;
    }

    m_memory_ = MapViewOfFile(m_file_map_, m_only_read_ ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
#else
    if (m_only_read_ && access(file_name.c_str(), F_OK)==-1) {
	    return false;
    }

    m_file_ = open(file_name.c_str(), m_only_read_?(O_RDONLY):(O_RDWR|O_CREAT), 0660);
    if (m_file_==-1) {
	    return false;
    }

    struct stat	statv;

    if (fstat(m_file_, &statv)==-1) {
	    return false;
    }

    m_update_ = statv.st_mtime;
    m_file_size_ = statv.st_size;
    if (m_file_size_==0) {
	    m_file_size_ = file_size;
    }

    if (!m_only_read_) {
	    m_over_file_size_ = (m_file_size_ / m_granulate_ + 1) * m_granulate_;

        if (ftruncate(m_file_, m_over_file_size_)==-1) {
		    return false;
        }
    } else {
	    m_over_file_size_ = m_file_size_;
    }

    m_memory_ = mmap(0, m_over_file_size_, PROT_READ|(m_only_read_?0:PROT_WRITE), MAP_SHARED, m_file_, 0);
#endif

    return m_memory_ != nullptr;
}

void fastfile::set_length(const size_ff new_size) {
    if (new_size <= m_file_size_ || new_size <= m_over_file_size_) {
        m_file_size_ = new_size;
    } else {
#ifndef UNIX
        if (m_memory_ != nullptr) {
            UnmapViewOfFile(m_memory_);
            m_memory_ = nullptr;
        }

        if (m_file_map_ != nullptr) {
            CloseHandle(m_file_map_);
            m_file_map_ = nullptr;
        }

        m_file_size_ = new_size;
        m_over_file_size_ = (m_file_size_ / m_granulate_ + 1) * m_granulate_;

        m_file_map_ = CreateFileMapping(m_file_, nullptr, m_only_read_ ? PAGE_READONLY : PAGE_READWRITE, 0, m_over_file_size_, nullptr);
        if (m_file_map_ == nullptr) {
            return;
        }

        m_memory_ = MapViewOfFile(m_file_map_, m_only_read_ ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
#else
        if (m_memory_!=NULL) {
		    munmap(m_memory_, m_over_file_size_);
		    m_memory_ = NULL;
	    }

        m_file_size_ = new_size;
        m_over_file_size_ = (m_file_size_ / m_granulate_ + 1) * m_granulate_;

	    m_memory_ = mmap(0, m_over_file_size_, PROT_READ|(m_only_read_?0:PROT_WRITE), MAP_SHARED, m_file_, 0);
#endif
    }
}

void fastfile::close_file(size_ff size_on_close) {
#ifndef UNIX
    if (m_memory_ != nullptr) {
        UnmapViewOfFile(m_memory_);
        m_memory_ = nullptr;
    }

    if (m_file_map_ != nullptr) {
        CloseHandle(m_file_map_);
        m_file_map_ = nullptr;
    }

    if (m_file_ != INVALID_HANDLE_VALUE) {
        if (size_on_close == 0) {
            size_on_close = m_file_size_;
        }

        SetFilePointer(m_file_, size_on_close, 0, FILE_BEGIN);
        SetEndOfFile(m_file_);

        CloseHandle(m_file_);
        m_file_ = INVALID_HANDLE_VALUE;
    }
#else
    if (m_memory_ != nullptr) {
        munmap(m_memory_, m_over_file_size_);
        m_memory_ = nullptr;
    }

    if (m_file_ != -1) {
        if (size_on_close == 0) {
            size_on_close = m_file_size_;
        }

        if (ftruncate(m_file_, size_on_close) == -1) {
            // cout << errno;
        }

        close(m_file_);
        m_file_ = -1;
    }
#endif
}
