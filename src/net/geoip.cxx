#include "net/geoip.h"
#include "util/logger.h"

#ifdef WIN32
#pragma comment(lib, "GeoIP")
#endif // WIN32

using namespace khorost::network;

bool geo_ip_database::OpenDatabase(const std::string& sGeoiIPDatFile) {
    if (m_gi == nullptr) {
        LOG(INFO) << "Open geoip database '" << sGeoiIPDatFile << "'";
        m_gi = GeoIP_open(sGeoiIPDatFile.c_str(), GEOIP_STANDARD | GEOIP_SILENCE);
        if (m_gi == nullptr) {
            LOG(WARNING) << "Error open geoip database '" << sGeoiIPDatFile << "'";
            return false;
        }
        auto db_info = GeoIP_database_info(m_gi);
        if (db_info != nullptr) {
            LOG(INFO) << "Geoip database info - " << db_info;
        } else {
            LOG(WARNING) << "Error reading geoip database info";
        }
    }
    return true;
}

void geo_ip_database::CloseDatabase() {
    if (m_gi != nullptr) {
        GeoIP_delete(m_gi);
        m_gi = nullptr;
    }
}

std::string geo_ip_database::GetCountryCodeByIP(const std::string& sIP) {
    std::string sCountryCode = "--";

    if (m_gi != nullptr) {
        auto idx = GeoIP_database_edition(m_gi);
        int countryID = GeoIP_id_by_addr(m_gi, sIP.c_str());
        sCountryCode = GeoIP_country_code[countryID];

        LOG(DEBUG) << "GetCountryCodeByIP('" << sIP << "') = '" << sCountryCode << "'";
    }

    return sCountryCode;
}
