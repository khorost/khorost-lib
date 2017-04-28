#pragma once
#ifndef _GEOIP__H_
#define _GEOIP__H_

#include <string>
#include <GeoIP.h>

namespace khorost {
    namespace Network {
        class GeoIPDatabase {
            GeoIP*  m_gi;
        public:
            GeoIPDatabase():m_gi(nullptr) {
            }

            virtual ~GeoIPDatabase() { 
                CloseDatabase(); 
            }

            bool    OpenDatabase(const std::string& sGeoiIPDatFile);
            void    CloseDatabase();

            std::string GetCountryCodeByIP(const std::string& sIP);
        };
    }
}

#endif // _GEOIP__H_
