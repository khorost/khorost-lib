/***************************************************************************
*   Copyright (C) 2005 by khap                                            *
*   khap@khorost.com                                                      *
*                                                                         *
***************************************************************************/
#include <errmsg.h>

#include <boost/lexical_cast.hpp>

#include "db/mysql.h"
#include "util/logger.h"
//#include "kfunc.h"

#ifndef LOG_CTX_DATABASE
#define LOG_CTX_DATABASE "database"
#endif 

#pragma comment(lib,"libmysql.lib")

using namespace DB;

MySQL::MySQL():
	m_Handle(NULL){
}

MySQL::~MySQL(){
	Close();
}

bool MySQL::Connect(const std::string& rsHost_, unsigned int nPort_, const std::string& rsLogin_, const std::string& rsPassword_, const std::string& rsDatabase_, const std::string& rsConfigGroup_) {
	m_sHost			= rsHost_;
	m_nPort			= nPort_;
	m_sLogin		= rsLogin_;
	m_sPassword		= rsPassword_;
	m_sDatabase		= rsDatabase_;
	m_sConfigGroup	= rsConfigGroup_;

	return Reconnect();
}

bool MySQL::Reconnect(){
	Close();

	mysql_init(&m_mysql);
	if(!m_sConfigGroup.empty()) {
		mysql_options(&m_mysql, MYSQL_READ_DEFAULT_GROUP, m_sConfigGroup.c_str());
	}
	if (!(m_Handle = mysql_real_connect(&m_mysql, m_sHost.c_str(), m_sLogin.c_str(), m_sPassword.c_str(), m_sDatabase.c_str(), m_nPort, NULL, CLIENT_MULTI_RESULTS))) {
        LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[KMySQL::Reconnect()] error '%s'",mysql_error(&m_mysql));
		return false;
	}

	mysql_query(&m_mysql,"SET CHARACTER SET utf8");
	return true;
}

bool MySQL::Close(){
	if(m_Handle!=NULL){
		mysql_close(m_Handle);
		m_Handle = NULL;
	}

	return true;
}

bool MySQL::ExecuteSQL(const std::string& strSQL){
	int		iTry = 3;
	bool	bReconnectEnable = true;
m1:
	if(mysql_query(m_Handle,strSQL.c_str())){
		int iErrorCode = mysql_errno(m_Handle);

		iTry--;
		if(iTry!=0){
			if(iErrorCode==CR_SERVER_GONE_ERROR || iErrorCode==CR_SERVER_LOST){
				mysql_query(m_Handle,"SET CHARACTER SET utf8");

				LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[KMySQL::ExecuteSQL()] repair after '%d' error",iErrorCode);
				goto m1;
			}
		}
		if(bReconnectEnable && Reconnect()){
			bReconnectEnable = false;
			LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[KMySQL::ExecuteSQL()] use Reconnect repair after '%d' error",iErrorCode);
			goto m1;
		}
		LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[KMySQL::ExecuteSQL()] %d %s",iErrorCode,mysql_error(m_Handle));
		return false;
	}
	return true;
}

bool MySQL::Begin(){
	return ExecuteSQL("BEGIN");
}

bool MySQL::Commit(){
	return ExecuteSQL("COMMIT");
}

bool MySQL::Rollback(){
	return ExecuteSQL("ROLLBACK");
}

//	***********************************************************************
MySQL::Query::Query(MySQL* pConnection):
	m_Connection(pConnection),
	m_Handle(NULL),
	m_Result(NULL),
	m_Row(NULL){
	if(m_Connection!=NULL)
		m_Handle = m_Connection->m_Handle;
}

    MySQL::Query::Query(MySQL* pConnection,const std::string& strQuery):
	m_Connection(pConnection),
	m_Handle(NULL),
	m_Result(NULL),
	m_Row(NULL){
    if (m_Connection != NULL) {
        m_Handle = m_Connection->m_Handle;
    }
	AppendQuery(strQuery);
}

MySQL::Query::~Query() {
	if(m_Result!=NULL) {
		mysql_free_result(m_Result);
	}
}

void MySQL::Query::AppendQuery(const std::string& rsQuery_) {
	m_abQuery.Append(rsQuery_.c_str(), rsQuery_.size());
}

void MySQL::Query::ExpandValue(const char* psTag_, const std::string& rsValue_) {
	m_abQuery.Replace(psTag_, strlen(psTag_), rsValue_.c_str(), rsValue_.size());
}

void MySQL::Query::BindValue(const char* psTag_, const std::string& rsValue_) {
    std::string strTarget = std::string("\'") + rsValue_ + std::string("\'");
	m_abQuery.Replace(psTag_, strlen(psTag_), strTarget.c_str(), strTarget.size());
}

void MySQL::Query::BindValueBool(const char* psTag_, bool bValue_) {
	m_abQuery.Replace(psTag_, strlen(psTag_), (bValue_?"1":"0"), 1);
}

void MySQL::Query::BindValue(const char* psTag_, uint32_t nValue_) {
    std::string strTarget = boost::lexical_cast<std::string>(nValue_);
	m_abQuery.Replace(psTag_, strlen(psTag_), strTarget.c_str(), strTarget.size());
}

void MySQL::Query::BindValue64(const char* psTag_, uint64_t nValue_) {
    std::string strTarget = boost::lexical_cast<std::string>(nValue_);
	m_abQuery.Replace(psTag_, strlen(psTag_), strTarget.c_str(), strTarget.size());
}

void MySQL::Query::BindValueBLOB(const char* psTag_, const Data::AutoBufferT<uint8_t>& abBLOB_) {
	Data::AutoBufferChar	abTemp;
	abTemp.CheckSize(abBLOB_.GetFillSize()*2 + 1 + 2);
	uint32_t uiReplaceSize = mysql_real_escape_string(m_Handle, abTemp.GetPosition(1), (char*)abBLOB_.GetPosition(0), abBLOB_.GetFillSize());
	abTemp.DecrementFreeSize(uiReplaceSize + 2);
	*(abTemp.GetPosition(0)) = '\'';
	*(abTemp.GetPosition(uiReplaceSize + 1)) = '\'';
	m_abQuery.Replace(psTag_, strlen(psTag_), abTemp.GetPosition(0), abTemp.GetFillSize());
}
/*
void MySQL::Query::BindValue(const char* strTag,const char* strTagMS,const System::DateTime& dtValue){
	BindValue(strTag,dtValue.GetFormatString("%Y-%m-%d %H:%M:%S"));
	if(strTagMS!=NULL){
		BindValue(strTagMS,dtValue.GetMilliSeconds());
	}
}
*/
void MySQL::Query::Execute(){
	int		iTry = 3;
	bool	bReconnectEnable = true;

    // нужна z-строка, а то ошибки ползут
    *m_abQuery.GetPosition(m_abQuery.GetFillSize()) = '\0';
m1:
	if(mysql_real_query(m_Handle, (const char*)m_abQuery.GetPosition(0), m_abQuery.GetFillSize())){
		int iErrorCode = mysql_errno(m_Handle);

		iTry--;
		if(iTry!=0){
			if(iErrorCode==CR_SERVER_GONE_ERROR || iErrorCode==CR_SERVER_LOST){
				mysql_query(m_Handle,"SET CHARACTER SET utf8");

				LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[MySQL::Query::Execute()] repair after '%d' error",iErrorCode);
				goto m1;
			}
		}

		if(bReconnectEnable && m_Connection!=NULL && m_Connection->Reconnect()){
			bReconnectEnable = false;
			LOG_CONTEXT(LOG_CTX_DATABASE,LOG_LEVEL_DEBUG,"[MySQL::ExecuteSQL()] use Reconnect repair after '%d' error",iErrorCode);
			goto m1;
		}

        throw std::string("Execute failed: ") + boost::lexical_cast<std::string>(iErrorCode) + std::string(" - \"") + std::string(mysql_error(m_Handle)) + std::string("\"");
	}
}

bool MySQL::Query::StartFetch(){
	m_Result = mysql_store_result(m_Handle);
	if(m_Result!=NULL) {
		NextFetch();
		return IsFetchAvailable();
	} else if (mysql_field_count(m_Handle) == 0) {
		m_nRowsCount = static_cast<int>(mysql_affected_rows(m_Handle));
		return true;
	} else {
		return false;
	}
}

void MySQL::Query::NextFetch(){
	m_Row = mysql_fetch_row(m_Result);
}

bool MySQL::Query::IsFetchAvailable(){
	return m_Row!=NULL;
}

bool MySQL::Query::IsNextResult() {
	return mysql_next_result(m_Handle) == 0;
}

bool MySQL::Query::IsNull(int position) const {
	return m_Row[position]==NULL;
}

bool MySQL::Query::IsTrue(int position){
	return IsNull(position)?false:(m_Row[position][0]=='1');
}

int MySQL::Query::GetInteger(int position){
    return IsNull(position) ? 0 : boost::lexical_cast<int>(m_Row[position]);
}

uint64_t MySQL::Query::GetInteger64(int position){
    return IsNull(position) ? 0 : boost::lexical_cast<uint64_t>(m_Row[position]);
}

const char*	MySQL::Query::GetString(int position){
	return IsNull(position)?"":m_Row[position];
}

uint32_t MySQL::Query::GetLastAutoIncrement(){
	return (uint32_t)mysql_insert_id(m_Handle);
}
/*
const System::DateTime MySQL::Query::GetDateTime(int position,int positionMS) const {
	return IsNull(position)?System::DateTime():System::DateTime("YYYY-MM-DD hh:mm:ss",m_Row[position],positionMS!=-1?m_Row[positionMS]:"");
}
*/
