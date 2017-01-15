#ifndef __FRAMEWORK__
#define __FRAMEWORK__

#ifdef UNIX
#else
#include <windows.h>
#endif

#include "util/logger.h"
#include "app/server2hb.h"

#define FRAMEWORK_2HTTP_BINARY_SERVER( TSERVER ) \
int main(int argc_, char* argv_[]) { \
	TSERVER _Server_; \
    int     nResult = EXIT_SUCCESS; \
	std::unique_ptr<g3::LogWorker>  logger(g3::LogWorker::createLogWorker()); \
	g3::initializeLogging(logger.get()); \
    khorost::log::appendColorSink(logger.get()); \
	LOG(INFO) << "Start application";\
    if (!_Server_.CheckParams(argc_, argv_, nResult, logger.get())){\
        return nResult;\
    }\
    if (_Server_.PrepareToStart() && _Server_.AutoExecute() && _Server_.Startup()) {\
        _Server_.Run();\
    }\
    _Server_.Finish();\
    LOG(INFO) << "Terminate application";\
    return nResult; \
}

#endif // __FRAMEWORK__
