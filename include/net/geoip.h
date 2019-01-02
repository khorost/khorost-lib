#pragma once

#include <string>
#include <GeoIP.h>

namespace khorost {
    namespace network {
        class geo_ip_database {
            GeoIP* m_gi;
        public:
            geo_ip_database(): m_gi(nullptr) {
            }

            virtual ~geo_ip_database() {
                close_database();
            }

            bool open_database(const std::string& sGeoiIPDatFile);
            void close_database();

            std::string get_country_code_by_ip(const std::string& ip) const;
        };
    }
}
