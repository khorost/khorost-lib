#pragma once

#include <utility>
#include <spdlog/spdlog.h>

namespace khorost {
    namespace profiler {
        class cpu final {
        public:
            enum precision { seconds, milliseconds, microseconds, nanoseconds };

        private:
            boost::posix_time::ptime m_start;
            std::shared_ptr<spdlog::logger> m_logger;
            std::string m_tag;
            precision m_precision;
        public:
            cpu(std::shared_ptr<spdlog::logger> logger, std::string tag, const precision log_precision = microseconds) : m_logger(std::move(logger)), m_tag(std::move(tag)), m_precision(log_precision) {
                m_start = boost::posix_time::microsec_clock::universal_time();
            }

            ~cpu() {
                print();
            }

            void print() const {
                if (m_logger != nullptr) {
                    switch (m_precision) {
                    case nanoseconds:
                        m_logger->debug("[PROFILER] {} = {} nanoseconds", m_tag, (boost::posix_time::microsec_clock::universal_time() - m_start).total_nanoseconds());
                        break;
                    case microseconds:
                        m_logger->debug("[PROFILER] {} = {} microseconds", m_tag, (boost::posix_time::microsec_clock::universal_time() - m_start).total_microseconds());
                        break;
                    case milliseconds:
                        m_logger->debug("[PROFILER] {} = {} milliseconds", m_tag, (boost::posix_time::microsec_clock::universal_time() - m_start).total_milliseconds());
                        break;
                    case seconds:
                    default:
                        m_logger->debug("[PROFILER] {} = {} seconds", m_tag, (boost::posix_time::microsec_clock::universal_time() - m_start).total_seconds());
                        break;
                    }
                }
            }
        };

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if !(defined(__PRETTY_FUNCTION__))
#define __PRETTY_FUNCTION__   __FUNCTION__
#endif

#define PROFILER_FUNCTION(LOGGER, ...)   khorost::profiler::cpu __cpu_profiler__(LOGGER, __FUNCTION__ + std::string(" (" __FILE__ ":") + std::string(TOSTRING(__LINE__) ")"), ## __VA_ARGS__)
#define PROFILER_TAG(LOGGER, TAG, ...)   khorost::profiler::cpu __cpu_profiler__##TAG(LOGGER, #TAG, ## __VA_ARGS__)
    }
}
