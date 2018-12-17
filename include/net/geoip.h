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
                CloseDatabase();
            }

            bool OpenDatabase(const std::string& sGeoiIPDatFile);
            void CloseDatabase();

            std::string GetCountryCodeByIP(const std::string& sIP);
        };
    }
}
