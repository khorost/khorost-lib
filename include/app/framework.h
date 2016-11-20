#ifndef __FRAMEWORK__
#define __FRAMEWORK__

#ifdef UNIX
#else
#include <windows.h>
#endif

#include "util/logger.h"
#include "app/server2hb.h"

#define FRAMEWORK_2HTTP_BINARY_SERVER( TSERVER ) \
TSERVER g__Server_; \
int main(int argc_, char* argv_[]) { \
    khorost::log::prepare(); \
	LOGF(INFO, "Start application");\
    if (!g__Server_.Prepare(argc_, argv_)){\
        return -1;\
    }\
    if (g__Server_.Startup()) {\
        g__Server_.Run();\
    }\
    g__Server_.Finish();\
/*    LOGF(INFO, "Terminate application");*/\
    return 0; \
}

#endif // __FRAMEWORK__
