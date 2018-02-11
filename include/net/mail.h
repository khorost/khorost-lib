#pragma once

#include <string>

#include <curl/curl.h>

namespace khorost {
    namespace Network {
        class smtp {
            std::string m_sHost;
            std::string m_sUser;
            std::string m_sPassword;
        public:
            smtp(){
            }

            virtual ~smtp() {
            }

            void register_connect(const std::string& sHost_, const std::string& sUser_, const std::string& sPassword_);

            bool send_message(const std::string& sTo_, const std::string& sFrom_, const std::string& sSubject_, const std::string& sBody_);
        };
    }
}

