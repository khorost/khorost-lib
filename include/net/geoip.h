#pragma once

#ifndef _NO_USE_MMDB
#include <string>
#include <maxminddb.h>

namespace khorost {
    namespace network {
        class geo_ip_database {
            MMDB_s m_mmdb_;
        public:
            geo_ip_database() {
            }

            virtual ~geo_ip_database() {
                close_database();
            }

            static std::string get_lib_version_mmdb() { return MMDB_lib_version(); }

            bool open_database(const std::string& path_name);
            void close_database();

            std::string get_country_code_by_ip(const std::string& ip) const;
        };
    }
}

#endif // _NO_USE_MMDB
