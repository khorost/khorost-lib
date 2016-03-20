#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <map>

/************************************************************************/
/* Использование логера log4cxx                                         */
/************************************************************************/
// здесь необходимо включать и отключать использование log4cxx
//#define LOG4CXX_USE

#ifdef LOG4CXX_USE

#include <log4cxx/logstring.h>
#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#define LOG_LEVEL_FATAL	(log4cxx::Level::FATAL_INT)
#define LOG_LEVEL_ERROR	(log4cxx::Level::ERROR_INT)
#define LOG_LEVEL_WARN	(log4cxx::Level::WARN_INT)
#define LOG_LEVEL_INFO	(log4cxx::Level::INFO_INT)
#define LOG_LEVEL_DEBUG	(log4cxx::Level::DEBUG_INT)

namespace khorost {
    namespace Log{

        class Context{
            typedef std::map<std::string, int>	ContextNumberDict;

            std::string			m_sDefaultContext;	// имя общего контекста логирования
            ContextNumberDict	m_NumberDict;		// уровень логирования
        public:
            Context(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_);
            virtual ~Context();

            enum ContextStack{
                CONTEXT_STACK_NONE,
                CONTEXT_STACK_UP,
                CONTEXT_STACK_DOWN,
            };

            void Prepare(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_);

            void LogStrVA(ContextStack ContextStack_, int iLevel_, const char* sFormat_, ...);
            void LogStrVA(ContextStack ContextStack_, const std::string& sContext_, int iLevel_, const char* sFormat_, ...);

            void LogStr(ContextStack ContextStack_, int iLevel_, const std::string& sInfo_);
            void LogStr(ContextStack ContextStack_, const std::string& sContext_, int iLevel_, const std::string& sInfo_);

            std::string	GetDefaultContext() const { return m_sDefaultContext; }
            int		GetStackLevel(const std::string& sContext_) const;
            void	SetStackLevel(const std::string& sContext_, int StackLevel_);
            void	ProcessStackLevel(const std::string& sContext_, ContextStack ContextStack_);
        };

        class ContextAuto{
            Context&		m_Context;
            std::string		m_sDescription;
            int				m_iLevel;
            std::string		m_sContext;
        public:
            ContextAuto(Context& Context_, const std::string& sContext_, int iLevel_, const std::string& sDescription_);
            virtual ~ContextAuto();
        };

        // глобальный логер
        extern Context		g_Logger;
    }
}

#define MAX_LOG4CXX_FORMAT_BUFFER		0x400
#define STACK_LEVEL_FILL				'.'

#endif	// LOG4CXX_USE


#ifdef LOG4CXX_USE
#define LOG_CONTEXT(context, level, format, ...)					Log::g_Logger.LogStrVA(Log::Context::CONTEXT_STACK_NONE, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_ENTER(context, level, format, ...)			    Log::g_Logger.LogStrVA(Log::Context::CONTEXT_STACK_UP, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_LEAVE(context, level, format, ...)			    Log::g_Logger.LogStrVA(Log::Context::CONTEXT_STACK_NONE, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_AUTO(context, level, info)						Log::ContextAuto ___lca_A(Log::g_Logger, context, level, info)

#define LOG_FATAL(logger, info)										LOG4CXX_FATAL(logger, info)
#define LOG_ERROR(logger, info)										LOG4CXX_ERROR(logger, info)
#define LOG_WARN(logger, info)										LOG4CXX_WARN(logger, info)
#define LOG_INFO(logger, info)										LOG4CXX_INFO(logger, info)
#define LOG_DEBUG(logger, info)										LOG4CXX_DEBUG(logger, info)
#else
#define LOG_CONTEXT(context, level, format, ...)
#define LOG_CONTEXT_ENTER(context, level, format, ...)
#define LOG_CONTEXT_LEAVE(context, level, format, ...)
#define LOG_CONTEXT_AUTO(context, level, format, ...)

#define LOG_FATAL(logger, info)
#define LOG_ERROR(logger, info)
#define LOG_WARN(logger, info)
#define LOG_INFO(logger, info)
#define LOG_DEBUG(logger, info)
#endif	// LOG4CXX_USE

#endif	// __LOGGER_H__
