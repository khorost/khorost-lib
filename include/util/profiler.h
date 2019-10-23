#pragma once

#include <utility>
#include <spdlog/spdlog.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace khorost {
    namespace profiler {
        class cpu final {
        public:
            enum precision { seconds, milliseconds, microseconds, nanoseconds };

        private:
            boost::posix_time::ptime m_start_;
            std::shared_ptr<spdlog::logger> m_logger_;
            std::string m_tag_;
            precision m_precision_;
        public:
            cpu(std::shared_ptr<spdlog::logger> logger, std::string tag, const precision log_precision = microseconds) : m_logger_(std::move(logger)), m_tag_(std::move(tag)), m_precision_(log_precision) {
                m_start_ = boost::posix_time::microsec_clock::universal_time();
            }

            ~cpu() {
                print();
            }

            void print() const {
                if (m_logger_ != nullptr) {
                    const auto dt = boost::posix_time::microsec_clock::universal_time() - m_start_;
                    switch (m_precision_) {
                    case nanoseconds:
                        m_logger_->debug("[PROFILER] {:>8} nanoseconds {}", dt.total_nanoseconds(), m_tag_);
                        break;
                    case microseconds:
                        m_logger_->debug("[PROFILER] {:>8} microseconds {}", dt.total_microseconds(), m_tag_);
                        break;
                    case milliseconds:
                        m_logger_->debug("[PROFILER] {:>8} milliseconds {}", dt.total_milliseconds(), m_tag_);
                        break;
                    case seconds:
                    default:
                        m_logger_->debug("[PROFILER] {:>8} seconds {}", dt.total_seconds(), m_tag_);
                        break;
                    }
                }
            }
        };

#if !(defined(STRINGIFY))
#define STRINGIFY(x) #x
#endif
#define TOSTRING(x) STRINGIFY(x)

#if !(defined(__PRETTY_FUNCTION__))
#define __PRETTY_FUNCTION__   __FUNCTION__
#endif

#define PROFILER_FUNCTION_TAG(LOGGER, TAG, ...)   khorost::profiler::cpu __cpu_profiler__(LOGGER, TAG + " " + __PRETTY_FUNCTION__ + std::string(" (" __FILE__ ":") + std::string(TOSTRING(__LINE__) ")"), ## __VA_ARGS__)
#define PROFILER_FUNCTION(LOGGER, ...)   khorost::profiler::cpu __cpu_profiler__(LOGGER, __PRETTY_FUNCTION__ + std::string(" (" __FILE__ ":") + std::string(TOSTRING(__LINE__) ")"), ## __VA_ARGS__)
#define PROFILER_TAG(LOGGER, TAG, ...)   khorost::profiler::cpu __cpu_profiler__##TAG(LOGGER, #TAG, ## __VA_ARGS__)
    }
}
