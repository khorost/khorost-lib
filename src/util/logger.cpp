
#ifdef WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

#include "util/logger.h"

/************************************************************************/
/* Использование логера log4cxx                                         */
/************************************************************************/
#ifdef LOG4CXX_USE

#ifdef WIN32

#ifdef LOG4CXX_STATIC
#if _DEBUG
# pragma comment(lib,"log4cxx_static_debug.lib")
# pragma comment(lib,"apr-1_static_debug.lib")
# pragma comment(lib,"aprutil-1_static_debug.lib")
# pragma comment(lib,"xml_static_debug.lib")
#else
# pragma comment(lib,"log4cxx_static_release.lib")
# pragma comment(lib,"apr-1_static_release.lib")
# pragma comment(lib,"aprutil-1_static_release.lib")
# pragma comment(lib,"xml_static_release.lib")
#endif
#else
#if _DEBUG
# pragma comment(lib,"log4cxx_dll_debug.lib")
# pragma comment(lib,"apr-1_dll_debug.lib")
# pragma comment(lib,"aprutil-1_dll_debug.lib")
# pragma comment(lib,"xml_dll_debug.lib")
#else
# pragma comment(lib,"log4cxx_dll_release.lib")
# pragma comment(lib,"apr-1_dll_release.lib")
# pragma comment(lib,"aprutil-1_dll_release.lib")
# pragma comment(lib,"xml_dll_release.lib")
#endif
#endif
#pragma comment(lib,"wsock32.lib")
#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib,"ODBC32.LIB")

#endif // WIN32

using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace Log;

Log::Context		Log::g_Logger("common","log4cxx.properties", ".\\");

Context::Context(const std::string& _sDefaultContext, const std::string& _sPropertyFilename, const std::string& _sDefaultDirectory){
	Prepare(_sDefaultContext, _sPropertyFilename, _sDefaultDirectory);
}

Context::~Context(){
}

void Context::Prepare(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_) {
    using namespace boost::filesystem;

    m_sDefaultContext = sDefaultContext_;

    path filePath, currentPath = current_path();

    filePath = currentPath;
    filePath /= sPropertyFilename_;

    if (!exists(filePath)) {
        filePath = sDefaultDirectory_;
        filePath /= sPropertyFilename_;

        if (!exists(filePath)) {
            return;
        }
    }

    PropertyConfigurator::configure(File(filePath.string()));

    LogStrVA(
        CONTEXT_STACK_NONE
        , "common"
        , LOG_LEVEL_INFO
        , "Context::Prepare('%s', '%s') currentPath='%s'"
        , sDefaultContext_.c_str()
        , filePath.string().c_str()
        , currentPath.string().c_str());
}

void Context::LogStrVA(ContextStack _ContextStack, int _iLevel, const char* _sFormat,...) {
	va_list va;

	va_start(va, _sFormat);
	LogStrVA(_ContextStack, m_sDefaultContext, _iLevel, _sFormat, va);
	va_end(va);
}

void Context::LogStrVA(ContextStack _ContextStack, const std::string& _sContext, int _iLevel, const char* _sFormat,...){
	char	buffer[MAX_LOG4CXX_FORMAT_BUFFER], prefix[2*MAX_LOG4CXX_FORMAT_BUFFER] = "";
	va_list va;

	LoggerPtr logger = Logger::getLogger(_sContext);

	ProcessStackLevel(_sContext, _ContextStack);

	int k, cnt = GetStackLevel(_sContext);
	if(_ContextStack==CONTEXT_STACK_UP){
		cnt--;
	}
	for(k=0;k<cnt;){
		prefix[k++] = STACK_LEVEL_FILL;
	}
	prefix[k] = '\0';

	va_start(va, _sFormat);

	switch(_iLevel) {
		case LOG_LEVEL_FATAL:
			if ( logger->isFatalEnabled())  {
				vsnprintf(buffer, sizeof(buffer), _sFormat, va);
				strcat(prefix, buffer);
				LOG4CXX_FATAL(logger, prefix);
			}
			break;
		case LOG_LEVEL_ERROR:	
			if ( logger->isErrorEnabled())  {
				vsnprintf(buffer, sizeof(buffer), _sFormat, va);
				strcat(prefix, buffer);
				LOG4CXX_ERROR(logger, prefix);
			}
			break;
		case LOG_LEVEL_WARN:
			if ( logger->isWarnEnabled())  {
				vsnprintf(buffer, sizeof(buffer), _sFormat, va);
				strcat(prefix, buffer);
				LOG4CXX_WARN(logger, prefix);
			}
			break;
		case LOG_LEVEL_INFO:
			if ( logger->isInfoEnabled())  {
				vsnprintf(buffer, sizeof(buffer), _sFormat, va);
				strcat(prefix, buffer);
				LOG4CXX_INFO(logger, prefix);
			}
			break;
		case LOG_LEVEL_DEBUG:
			if ( logger->isDebugEnabled())  {
				vsnprintf(buffer, sizeof(buffer), _sFormat, va);
				strcat(prefix, buffer);
				LOG4CXX_DEBUG(logger, prefix);
			}
			break;
	};

	va_end(va);
}

void Context::LogStr(ContextStack _ContextStack, int _iLevel, const std::string& _sInfo){
	LogStr(_ContextStack, m_sDefaultContext, _iLevel, _sInfo);
}

void Context::LogStr(ContextStack _ContextStack, const std::string& _sContext, int _iLevel, const std::string& _sInfo){
	char		prefix[MAX_LOG4CXX_FORMAT_BUFFER] = "";
	LoggerPtr logger = Logger::getLogger(_sContext);

	ProcessStackLevel(_sContext, _ContextStack);

	int k, cnt = GetStackLevel(_sContext);
	if(_ContextStack==CONTEXT_STACK_UP){
		cnt--;
	}
	for(k=0;k<cnt;){
		prefix[k++] = STACK_LEVEL_FILL;
	}
	prefix[k] = '\0';

	switch(_iLevel) {
		case LOG_LEVEL_FATAL:
			LOG4CXX_FATAL(logger, std::string(prefix) + _sInfo);
			break;
		case LOG_LEVEL_ERROR:	
			LOG4CXX_ERROR(logger, std::string(prefix) + _sInfo);
			break;
		case LOG_LEVEL_WARN:
			LOG4CXX_WARN(logger, std::string(prefix) + _sInfo);
			break;
		case LOG_LEVEL_INFO:
			LOG4CXX_INFO(logger, std::string(prefix) + _sInfo);
			break;
		case LOG_LEVEL_DEBUG:
			LOG4CXX_DEBUG(logger, std::string(prefix) + _sInfo);
			break;
	};
}

int	Context::GetStackLevel(const std::string& _sContext) const{
	ContextNumberDict::const_iterator	cit = m_NumberDict.find(_sContext);
	return cit!=m_NumberDict.end()?cit->second:0;
}

void Context::SetStackLevel(const std::string& _sContext, int _StackLevel){
	m_NumberDict[_sContext] = _StackLevel;
}

void Context::ProcessStackLevel(const std::string& _sContext, ContextStack _ContextStack){
	if(_ContextStack==CONTEXT_STACK_UP){
		SetStackLevel(_sContext, GetStackLevel(_sContext) + 1);
	}else if(_ContextStack==CONTEXT_STACK_DOWN){
		SetStackLevel(_sContext, GetStackLevel(_sContext) - 1);
	}
}

ContextAuto::ContextAuto(Log::Context &_Context, const std::string& _sContext, int _iLevel, const std::string &_sDescription):
	m_Context(_Context)
	,m_sDescription(_sDescription)
	,m_iLevel(_iLevel)
	,m_sContext(_sContext){
		m_Context.LogStr(Context::CONTEXT_STACK_UP, m_sContext, m_iLevel, std::string("{E}") + m_sDescription);
}

ContextAuto::~ContextAuto(){
	m_Context.LogStr(Context::CONTEXT_STACK_DOWN, m_sContext, m_iLevel, std::string("{L}") + m_sDescription);
}

#endif
