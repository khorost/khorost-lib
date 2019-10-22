#pragma once

#include <string>

namespace khorost {
    namespace network {
        class stomp final {
        public:
            bool parse(const std::string& message) {
                auto pos = message.find_first_of("\n", 0);
                if (pos == std::string::npos) {
                    return false;
                }

                auto end_pos = message[pos - 1] == '\r' ? (pos - 1) : pos;
                m_command_ = message.substr(0, end_pos);

                auto start_pos = pos + 1;
                while (start_pos < message.size()) {
                    if (message[start_pos] != '\n' || message[start_pos] != '\r') {
                        start_pos += message[start_pos] == '\r' ? 2 : 1;
                        m_body_ = message.substr(start_pos);

                        return true;
                    }

                    pos = message.find_first_of(":", start_pos);
                    if (pos == std::string::npos) {
                        return false; // error parsing
                    }
                    const auto key = message.substr(start_pos, pos - start_pos);
                    start_pos = pos + 1;
                    pos = message.find_first_of("\n", start_pos);
                    if (pos == std::string::npos) {
                        return false; // error parsing
                    }

                    end_pos = message[pos - 1] == '\r' ? (pos - 1) : pos;
                    const auto value = message.substr(start_pos, end_pos);

                    m_headers_.insert(std::pair<std::string, std::string>(key, value));
                    start_pos = pos + 1;
                }

                return true;
            }

            const std::string& get_command() const { return m_command_; }
            std::string get_connect_response() {
                return "CONNECTED\nversion:1.2\n\n";
            }

        private:
            std::string m_command_;
            std::map<std::string, std::string> m_headers_;
            std::string m_body_;
        };
    }
}
