#pragma once

#include <string>
#include <utility>

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>

#include "app/config.h"
#include "app/khl-define.h"

namespace khorost {
    namespace network {
        class token final {
            std::string m_access_token_;
            std::string m_refresh_token_;
            boost::posix_time::ptime m_access_expire_;
            boost::posix_time::ptime m_refresh_expire_;
            Json::Value m_payload_;
        public:
            token(std::string access_token, const boost::posix_time::ptime& access_expire,
                  std::string refresh_token, const boost::posix_time::ptime& refresh_expire,
                  Json::Value payload):
                m_access_token_(std::move(access_token))
                , m_refresh_token_(std::move(refresh_token))
                , m_access_expire_(access_expire)
                , m_refresh_expire_(refresh_expire)
                , m_payload_(std::move(payload)) {

            }

            token(std::string access_token, const std::string& access_expire, std::string refresh_token,
                  const std::string& refresh_expire, Json::Value payload) :
                m_access_token_(std::move(access_token))
                , m_refresh_token_(std::move(refresh_token))
                , m_payload_(std::move(payload)) {
                m_access_expire_ = boost::posix_time::time_from_string(access_expire);
                m_refresh_expire_ = boost::posix_time::time_from_string(refresh_expire);
            }

            std::string get_access_token() const { return m_access_token_; }
            std::string get_refresh_token() const { return m_refresh_token_; }

            bool is_no_expire_access(const boost::posix_time::ptime& check) const { return check < m_access_expire_; }

            bool is_no_expire_refresh(const boost::posix_time::ptime& check) const { return check < m_refresh_expire_; }

            const Json::Value& get_payload() const { return m_payload_; }

            const boost::posix_time::ptime& get_access_expire() const { return m_access_expire_; }
            const boost::posix_time::ptime& get_refresh_expire() const { return m_refresh_expire_; }

            int get_access_duration() const;
            int get_refresh_duration() const;

            void set_access_duration(const int seconds) {
                m_payload_[khl_json_param_delta_access_time] = seconds;
            }

            void set_refresh_duration(const int seconds) {
                m_payload_[khl_json_param_delta_refresh_time] = seconds;
            }

            void set_access_token(const std::string& access_token) {
                m_access_token_ = access_token;
                m_payload_[khl_json_param_access_token] = access_token;
            }

            void set_refresh_token(const std::string& refresh_token) {
                m_refresh_token_ = refresh_token;
                m_payload_[khl_json_param_refresh_token] = refresh_token;
            }

            void set_access_expire(const boost::posix_time::ptime& access_expire) {
                m_access_expire_ = access_expire;
                m_payload_[khl_json_param_access_expire] = to_iso_extended_string(access_expire);
            }

            void set_refresh_expire(const boost::posix_time::ptime& refresh_expire) {
                m_refresh_expire_ = refresh_expire;
                m_payload_[khl_json_param_refresh_expire] = to_iso_extended_string(refresh_expire);
            }

            void set_payload(const std::string& key, const Json::Value& value) {
                m_payload_[key] = value;
            }
        };

        typedef std::shared_ptr<token> token_ptr;
    }
}
