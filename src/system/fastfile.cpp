// FastFile.cpp: implementation of the CFastFile class.
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

using namespace khorost::System;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FastFile::FastFile(size_ff nGranulate_):
	m_nGranulate(nGranulate_)
	, m_pMemory(NULL)
    , m_nFileSize(0)
    , m_nOverFileSize(0)
{
#ifndef UNIX
	m_hFile		= INVALID_HANDLE_VALUE;
	m_hFileMap	= NULL;

	if(m_nGranulate==0) {
		SYSTEM_INFO		SI;
		GetSystemInfo(&SI);
		m_nGranulate = SI.dwAllocationGranularity;
	}
#else
	m_hFile		= -1;
	if(m_nGranulate==0)
		m_nGranulate = sysconf(_SC_PAGESIZE);
#endif
}

FastFile::~FastFile() {
	Close();
}

bool FastFile::Open(const std::string& sName_, size_ff nSize_, bool bOnlyRead_) {
    Close();

    m_bOnlyRead = bOnlyRead_;

#ifndef UNIX
    if (bOnlyRead_ && GetFileAttributes(sName_.c_str())==-1) {
	    return false;
    }

    m_hFile = CreateFile(sName_.c_str(), m_bOnlyRead?GENERIC_READ:GENERIC_WRITE|GENERIC_READ,
	    0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_hFile==INVALID_HANDLE_VALUE) {
	    return false;
    }

    FILETIME    ftCreate, ftAccess, ftWrite;
    SYSTEMTIME stUTC;

    GetFileTime(m_hFile, &ftCreate, &ftAccess, &ftWrite);
    FileTimeToSystemTime(&ftWrite, &stUTC);

    tm			_tm;

    memset(&_tm,0,sizeof(tm));

    _tm.tm_year		= stUTC.wYear - 1900;
    _tm.tm_mon		= stUTC.wMonth - 1;
    _tm.tm_mday		= stUTC.wDay;
    _tm.tm_hour		= stUTC.wHour;
    _tm.tm_min		= stUTC.wMinute;
    _tm.tm_sec		= stUTC.wSecond;

    _tm.tm_isdst	= -1;

    m_tUpdate = _mkgmtime(&_tm);

    m_nFileSize = GetFileSize(m_hFile, NULL);
    if (m_nFileSize==0) {
	    m_nFileSize = nSize_;
    }

    if (!m_bOnlyRead) {
	    m_nOverFileSize = (m_nFileSize / m_nGranulate + 1) * m_nGranulate;
    } else {
	    m_nOverFileSize = m_nFileSize;
    }

    m_hFileMap	= CreateFileMapping(m_hFile, NULL, m_bOnlyRead?PAGE_READONLY:PAGE_READWRITE, 0, m_nOverFileSize, NULL);
    if (m_hFileMap==NULL) {
	    return false;
    }

    m_pMemory = MapViewOfFile(m_hFileMap, m_bOnlyRead?FILE_MAP_READ:FILE_MAP_WRITE, 0, 0, 0);
#else
    if (m_bOnlyRead && access(sName_.c_str(), F_OK)==-1) {
	    return false;
    }

    m_hFile = open(sName_.c_str(), m_bOnlyRead?(O_RDONLY):(O_RDWR|O_CREAT), 0660);
    if (m_hFile==-1) {
	    return false;
    }

    struct stat	statv;

    if (fstat(m_hFile,&statv)==-1) {
	    return false;
    }

    m_tUpdate = statv.st_mtime;
    m_nFileSize = statv.st_size;
    if (m_nFileSize==0) {
	    m_nFileSize = nSize_;
    }

    if (!m_bOnlyRead) {
	    m_nOverFileSize = (m_nFileSize / m_nGranulate + 1) * m_nGranulate;

        if (ftruncate(m_hFile, m_nOverFileSize)==-1) {
		    return false;
        }
    } else {
	    m_nOverFileSize = m_nFileSize;
    }

    m_pMemory = mmap(0, m_nOverFileSize, PROT_READ|(m_bOnlyRead?0:PROT_WRITE), MAP_SHARED, m_hFile, 0);
#endif

    return m_pMemory!=NULL;
}

void FastFile::SetLength(size_ff nNewSize_) {
    if (nNewSize_ <= m_nFileSize || nNewSize_<=m_nOverFileSize) {
	    m_nFileSize = nNewSize_;
    } else {
#ifndef UNIX
	    if (m_pMemory!=NULL) {
		    UnmapViewOfFile(m_pMemory);
		    m_pMemory = NULL;
	    }

	    if (m_hFileMap!=NULL) {
		    CloseHandle(m_hFileMap);
		    m_hFileMap = NULL;
	    }

        m_nFileSize = nNewSize_;
        m_nOverFileSize = (m_nFileSize / m_nGranulate + 1) * m_nGranulate;

	    m_hFileMap	= CreateFileMapping(m_hFile, NULL, m_bOnlyRead?PAGE_READONLY:PAGE_READWRITE, 0, m_nOverFileSize, NULL);
        if (m_hFileMap==NULL) {
		    return;
        }

        m_pMemory = MapViewOfFile(m_hFileMap, m_bOnlyRead?FILE_MAP_READ:FILE_MAP_WRITE, 0, 0, 0);
#else
        if (m_pMemory!=NULL) {
		    munmap(m_pMemory, m_nOverFileSize);
		    m_pMemory = NULL;
	    }

        m_nFileSize = nNewSize_;
        m_nOverFileSize = (m_nFileSize / m_nGranulate + 1) * m_nGranulate;

	    m_pMemory = mmap(0, m_nOverFileSize, PROT_READ|(m_bOnlyRead?0:PROT_WRITE), MAP_SHARED, m_hFile, 0);
#endif
    }
}

void FastFile::Close(size_ff nSize_) {
#ifndef UNIX
    if (m_pMemory!=NULL) {
	    UnmapViewOfFile(m_pMemory);
	    m_pMemory = NULL;
    }

    if (m_hFileMap!=NULL) {
	    CloseHandle(m_hFileMap);
	    m_hFileMap = NULL;
    }

    if (m_hFile!=INVALID_HANDLE_VALUE) {
        if (nSize_==0) {
		    nSize_ = m_nFileSize;
        }

	    SetFilePointer(m_hFile, nSize_, 0, FILE_BEGIN);
	    SetEndOfFile(m_hFile);

	    CloseHandle(m_hFile);
	    m_hFile = INVALID_HANDLE_VALUE;
    }
#else
    if (m_pMemory!=NULL) {
	    munmap(m_pMemory, m_nOverFileSize);
	    m_pMemory = NULL;
    }

    if (m_hFile!=-1) {
        if (nSize_==0) {
		    nSize_ = m_nFileSize;
        }

	    ftruncate(m_hFile, nSize_);

	    close(m_hFile);
	    m_hFile = -1;
    }
#endif
}
