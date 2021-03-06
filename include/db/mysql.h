﻿/***************************************************************************
*   Copyright (C) 2005 by khap                                            *
*   khap@khorost.com                                                      *
*                                                                         *
***************************************************************************/
#ifndef __MYSQL_H__
#define __MYSQL_H__

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <WinSock2.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  

#include <mysql.h>

#include <string>
#include <boost/cstdint.hpp>

#include "util/autobuffer.h"

namespace khorost {
    namespace db {
	    class khl_mysql{
	    public:
		    class Query{
                khl_mysql*		            m_Connection;			// 
			    MYSQL*		            m_Handle;				// 
			    MYSQL_RES*				m_Result;				// 
			    MYSQL_ROW				m_Row;					// 
			    data::auto_buffer_char	m_abQuery;	// 
			    int			            m_nRowsCount;			// 
		    public:
			    Query(khl_mysql* pConnection_);
                Query(khl_mysql* pConnection_, const std::string& rsQuery_);
			    virtual ~Query();

			    void	AppendQuery(const std::string& rsQuery_);

			    void	ExpandValue(const char* psTag_, const std::string& rsValue_);

			    void	BindValue(const char* psTag_, const std::string& rsValue_);
			    void	BindValueBool(const char* psTag_, bool bValue_);
			    void	BindValue(const char* strTag, uint32_t uiValue);
			    void	BindValue64(const char* strTag,uint64_t uiValue);
			    void	BindValueBLOB(const char* strTag,const data::auto_buffer_t<uint8_t>& abBLOB_);
    //			void	BindValue(const char* strTag,const system::DateTime& dtValue){BindValue(strTag,NULL,dtValue);}
    //			void	BindValue(const char* strTag,const char* strTagMS,const system::DateTime& dtValue);

			    void	Execute();

			    uint32_t	GetLastAutoIncrement();

			    bool	StartFetch();
			    void	NextFetch();
			    bool	IsFetchAvailable();

			    bool	IsNextResult();

			    int		GetRowsCount() const {return m_nRowsCount;}

			    bool		IsNull(int position) const;
			    bool		IsTrue(int position);
			    int			GetInteger(int position);
			    uint64_t	GetInteger64(int position);
			    const char*	GetString(int position);
    //			const system::DateTime	GetDateTime(int position, int positionMS = -1) const;
		    };
	    protected:
		    MYSQL								m_mysql;		// 
		    MYSQL*								m_Handle;		// 
    //		system::SynchronizeCriticalSection	m_hLock;		// 
    /************************************************************************/
    /* Параметры подключения                                                */
    /************************************************************************/
		    std::string		m_sHost;
		    unsigned int	m_nPort;
		    std::string		m_sLogin;
		    std::string		m_sPassword;
		    std::string		m_sDatabase;
		    std::string		m_sConfigGroup;
	    public:
            khl_mysql();
		    virtual ~khl_mysql();

		    virtual bool	ExecuteSQL(const std::string& rsSQL_);
		    virtual bool	Begin();
		    virtual bool	Commit();
		    virtual bool	Rollback();

		    bool	Connect(const std::string& rsHost_, unsigned int nPort, const std::string& rsLogin_, const std::string& rsPassword_, const std::string& rsDatabase_, const std::string& rsConfigGroup_);
		    bool	Reconnect();
		    bool	Close();

	    };
    }
}

#endif // __MYSQL_H__
