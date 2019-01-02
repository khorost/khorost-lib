#pragma once

#ifndef UNIX
#include <windows.h>
#endif

#define FRAMEWORK_HTTP_APP_SERVER( TSERVER, LOGGER ) \
int main(int argc, char* argv[]) { \
	TSERVER _server_; \
    int result = EXIT_SUCCESS; \
    auto _logger_ = LOGGER; \
	_logger_->info("Start application");\
    if (!_server_.check_params(argc, argv, result)){\
        return result;\
    }\
    if (_server_.prepare_to_start() && _server_.auto_execute() && _server_.startup()) {\
        _server_.run();\
    }\
    _server_.finish();\
    _logger_->info("Terminate application");\
    return result; \
}
