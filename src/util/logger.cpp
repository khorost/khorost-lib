
#ifdef WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>

#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

#include "util/logger.h"

#pragma comment(lib,"dbghelp.lib")

#ifdef LOGGER_USE

khorost::Log::Context		khorost::Log::g_Logger("common", "log4cxx.properties", ".\\");

#ifdef WIN32

struct ColorCoutSinkWin32 {
    bool textcolorprotect = true;
    /*doesn't let textcolor be the same as backgroung color if true*/
    enum concol {
        black = 0,
        dark_blue = 1,
        dark_green = 2,
        dark_aqua, dark_cyan = 3,
        dark_red = 4,
        dark_purple = 5, dark_pink = 5, dark_magenta = 5,
        dark_yellow = 6,
        dark_white = 7,
        gray = 8,
        blue = 9,
        green = 10,
        aqua = 11, cyan = 11,
        red = 12,
        purple = 13, pink = 13, magenta = 13,
        yellow = 14,
        white = 15
    };

    int textcolor() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int a = csbi.wAttributes;
        return a % 16;
    }
    int backcolor() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int a = csbi.wAttributes;
        return (a / 16) % 16;
    }

    concol GetColor(const LEVELS level, concol defaultColor) const {
        if (level.value == WARNING.value) { return yellow; }
        if (level.value == DEBUG.value) { return green; }
        if (g3::internal::wasFatal(level)) { return red; }

        return defaultColor;
    }

    inline void setcolor(concol textcol, concol backcol) {
        setcolor(int(textcol), int(backcol));
    }

    inline void setcolor(int textcol, int backcol) {
        if (textcolorprotect) {
            if ((textcol % 16) == (backcol % 16))textcol++;
        }
        textcol %= 16; backcol %= 16;
        unsigned short wAttributes = ((unsigned)backcol << 4) | (unsigned)textcol;
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wAttributes);
    }

    void ReceiveLogMessage(g3::LogMessageMover logEntry) {
        auto level = logEntry.get()._level;
        auto defaultColor = concol(textcolor());
        auto color = GetColor(level, defaultColor);
        
        if (color != defaultColor) {
            setcolor(color, backcolor());
        }
        std::cout << logEntry.get().toString() /*<< std::endl*/;
        if (color != defaultColor) {
            setcolor(defaultColor, backcolor());
        }
    }
};

#else
struct ColorCoutSink {
    // Linux xterm color
    // http://stackoverflow.com/questions/2616906/how-do-i-output-coloured-text-to-a-linux-terminal
    enum FG_Color { YELLOW = 33, RED = 31, GREEN = 32, WHITE = 97 };

    FG_Color GetColor(const LEVELS level) const {
        if (level.value == WARNING.value) { return YELLOW; }
        if (level.value == DEBUG.value) { return GREEN; }
        if (g3::internal::wasFatal(level)) { return RED; }

        return WHITE;
    }

    void ReceiveLogMessage(g3::LogMessageMover logEntry) {
        auto level = logEntry.get()._level;
        auto color = GetColor(level);

        std::cout << "\033[" << color << "m"
            << logEntry.get().toString() << "\033[m" << std::endl;
    }
    };
#endif // WIN32

using namespace khorost::Log;

Context::Context(const std::string& _sDefaultContext, const std::string& _sPropertyFilename, const std::string& _sDefaultDirectory){
	Prepare();
}

Context::~Context(){
}

void Context::Prepare() {
    if (m_LogWorker == NULL) {
        m_LogWorker = g3::LogWorker::createLogWorker();
        g3::initializeLogging(m_LogWorker.get());
    }
#ifdef WIN32
    m_LogWorker->addSink(std::make_unique<ColorCoutSinkWin32>(), &ColorCoutSinkWin32::ReceiveLogMessage);
#else
    m_LogWorker->addSink(std::make_unique<ColorCoutSink>(), &ColorCoutSink::ReceiveLogMessage);
#endif  // WIN32
}

void Context::Prepare(const std::string& sDefaultContext_, const std::string& sPropertyFilename_, const std::string& sDefaultDirectory_) {
    using namespace boost::filesystem;

    m_sDefaultContext = sDefaultContext_;
    /*
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
    */
    if (m_LogWorker == NULL) {
        m_LogWorker = g3::LogWorker::createLogWorker();
        g3::initializeLogging(m_LogWorker.get());
    }
    auto handle = m_LogWorker->addDefaultLogger(sDefaultContext_, sDefaultDirectory_);
}

void Context::LogStrVA(eContextStack _ContextStack, int _iLevel, const char* _sFormat,...) {
	va_list va;

	va_start(va, _sFormat);
	LogStrVA(_ContextStack, m_sDefaultContext, _iLevel, _sFormat, va);
	va_end(va);
}

void Context::LogStrVA(eContextStack _ContextStack, const std::string& _sContext, int _iLevel, const char* _sFormat,...){
	char	buffer[MAX_LOG4CXX_FORMAT_BUFFER], prefix[2*MAX_LOG4CXX_FORMAT_BUFFER] = "";
	va_list va;

//	LoggerPtr logger = Logger::getLogger(_sContext);

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
        case LOG_LEVEL_ERROR:
            vsnprintf(buffer, sizeof(buffer), _sFormat, va);
            strcat(prefix, buffer);
            LOGF(FATAL, prefix);
			break;
		case LOG_LEVEL_WARN:
			vsnprintf(buffer, sizeof(buffer), _sFormat, va);
			strcat(prefix, buffer);
			LOGF(WARNING, prefix);
			break;
		case LOG_LEVEL_INFO:
			vsnprintf(buffer, sizeof(buffer), _sFormat, va);
			strcat(prefix, buffer);
			LOGF(INFO, prefix);
			break;
		case LOG_LEVEL_DEBUG:
			vsnprintf(buffer, sizeof(buffer), _sFormat, va);
			strcat(prefix, buffer);
			LOGF(DEBUG, prefix);
			break;
	};

    va_end(va);
}

void Context::LogStr(eContextStack _ContextStack, int _iLevel, const std::string& _sInfo){
	LogStr(_ContextStack, m_sDefaultContext, _iLevel, _sInfo);
}

void Context::LogStr(eContextStack _ContextStack, const std::string& _sContext, int _iLevel, const std::string& _sInfo){
	char		prefix[MAX_LOG4CXX_FORMAT_BUFFER] = "";
//	LoggerPtr logger = Logger::getLogger(_sContext);

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
        case LOG_LEVEL_ERROR:
            LOG(FATAL) << std::string(prefix) + _sInfo;
			break;
		case LOG_LEVEL_WARN:
			LOG(WARNING) << std::string(prefix) + _sInfo;
			break;
		case LOG_LEVEL_INFO:
			LOG(INFO) << std::string(prefix) + _sInfo;
			break;
		case LOG_LEVEL_DEBUG:
			LOG(DEBUG) << std::string(prefix) + _sInfo;
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

void Context::ProcessStackLevel(const std::string& sContext_, eContextStack ContextStack_){
	if(ContextStack_==CONTEXT_STACK_UP){
		SetStackLevel(sContext_, GetStackLevel(sContext_) + 1);
	}else if(ContextStack_==CONTEXT_STACK_DOWN){
		SetStackLevel(sContext_, GetStackLevel(sContext_) - 1);
	}
}

ContextAuto::ContextAuto(Context &Context_, const std::string& sContext_, int nLevel_, const std::string &sDescription_):
	m_Context(Context_)
	,m_sDescription(sDescription_)
	,m_iLevel(nLevel_)
	,m_sContext(sContext_){
		m_Context.LogStr(Context::CONTEXT_STACK_UP, m_sContext, m_iLevel, std::string("{E}") + m_sDescription);
}

ContextAuto::~ContextAuto(){
	m_Context.LogStr(Context::CONTEXT_STACK_DOWN, m_sContext, m_iLevel, std::string("{L}") + m_sDescription);
}

#endif
