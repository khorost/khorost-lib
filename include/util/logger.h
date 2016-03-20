#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <map>

/************************************************************************/
/* Использование логера                                                 */
/************************************************************************/
// здесь необходимо включать и отключать использование 
#define LOGGER_USE

#ifdef LOGGER_USE

#define G3_DYNAMIC_LOGGING
#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <g3log/std2_make_unique.hpp>

#define LOG_LEVEL_FATAL	1
#define LOG_LEVEL_ERROR	2
#define LOG_LEVEL_WARN	3
#define LOG_LEVEL_INFO	4
#define LOG_LEVEL_DEBUG	5

namespace khorost {
    namespace Log{
        class Context{
            typedef std::map<std::string, int>	ContextNumberDict;

            std::string			m_sDefaultContext;	// имя общего контекста логирования
            ContextNumberDict	m_NumberDict;		// уровень логирования

            std::unique_ptr<g3::LogWorker>  m_LogWorker;
        public:
            Context(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_);
            virtual ~Context();

            enum eContextStack{
                CONTEXT_STACK_NONE,
                CONTEXT_STACK_UP,
                CONTEXT_STACK_DOWN,
            };

            void Prepare();
            void Prepare(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_);

            void LogStrVA(eContextStack ContextStack_, int iLevel_, const char* sFormat_, ...);
            void LogStrVA(eContextStack ContextStack_, const std::string& sContext_, int iLevel_, const char* sFormat_, ...);

            void LogStr(eContextStack ContextStack_, int iLevel_, const std::string& sInfo_);
            void LogStr(eContextStack ContextStack_, const std::string& sContext_, int iLevel_, const std::string& sInfo_);

            std::string	GetDefaultContext() const { return m_sDefaultContext; }
            int		GetStackLevel(const std::string& sContext_) const;
            void	SetStackLevel(const std::string& sContext_, int StackLevel_);
            void	ProcessStackLevel(const std::string& sContext_, eContextStack ContextStack_);
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

#endif	// LOGGER_USE


#ifdef LOGGER_USE
#define LOG_CONTEXT(context, level, format, ...)					khorost::Log::g_Logger.LogStrVA(khorost::Log::Context::CONTEXT_STACK_NONE, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_ENTER(context, level, format, ...)			    khorost::Log::g_Logger.LogStrVA(khorost::Log::Context::CONTEXT_STACK_UP, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_LEAVE(context, level, format, ...)			    khorost::Log::g_Logger.LogStrVA(khorost::Log::Context::CONTEXT_STACK_NONE, context, level, format, ## __VA_ARGS__ )
#define LOG_CONTEXT_AUTO(context, level, info)						khorost::Log::ContextAuto ___lca_A(khorost::Log::g_Logger, context, level, info)

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
#endif	// LOGGER_USE

#endif	// __LOGGER_H__
