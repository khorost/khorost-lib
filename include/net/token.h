#pragma once

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include "app/config.h"

namespace khorost {
    namespace network {
        class token {
            std::string m_access_token;
            std::string m_refresh_token;
            boost::posix_time::ptime m_access_expire;
            boost::posix_time::ptime m_refresh_expire;
            Json::Value m_payload;
        public:
            token(const std::string& access_token, const boost::posix_time::ptime& access_expire,
                  const std::string& refresh_token, const boost::posix_time::ptime& refresh_expire,
                  const Json::Value& payload):
                m_access_token(access_token), m_refresh_token(refresh_token), m_access_expire(access_expire),
                m_refresh_expire(refresh_expire), m_payload(payload) {
            }

            std::string get_access_token() const { return m_access_token; }
            std::string get_refresh_token() const { return m_refresh_token; }

            bool is_no_expire_access(const boost::posix_time::ptime& check) const {
                return check < m_access_expire;
            }

            bool is_no_expire_refresh(const boost::posix_time::ptime& check) const {
                return check < m_refresh_expire;
            }

            Json::Value get_payload() const {
                return m_payload;
            }

            boost::posix_time::ptime get_access_expire() const { return m_access_expire; }
            boost::posix_time::ptime get_refresh_expire() const { return m_refresh_expire; }

            void set_access_token(const std::string& access_token) { m_access_token = access_token; }
            void set_refresh_token(const std::string& refresh_token) { m_refresh_token = refresh_token; }

            void set_access_expire(const boost::posix_time::ptime& access_expire) { m_access_expire = access_expire; }

            void set_refresh_expire(const boost::posix_time::ptime& refresh_expire) {
                m_refresh_expire = refresh_expire;
            }
        };

        typedef boost::shared_ptr<token> token_ptr;
    }
}
