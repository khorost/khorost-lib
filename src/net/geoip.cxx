#include "net/geoip.h"
#include "util/logger.h"
#include <app/khl-define.h>

#ifdef WIN32
#pragma comment(lib, "GeoIP")
#endif // WIN32

using namespace khorost::network;

bool geo_ip_database::open_database(const std::string& sGeoiIPDatFile) {
    if (m_gi == nullptr) {
        auto logger = spdlog::get(KHL_LOGGER_COMMON);

        logger->info("Open geoip database '{}'", sGeoiIPDatFile);

        m_gi = GeoIP_open(sGeoiIPDatFile.c_str(), GEOIP_STANDARD | GEOIP_SILENCE);
        if (m_gi == nullptr) {
            logger->warn("Error open geoip database '{}'", sGeoiIPDatFile);
            return false;
        }

        const auto db_info = GeoIP_database_info(m_gi);
        if (db_info != nullptr) {
            logger->info("Geoip database info - {}", db_info);
        } else {
            logger->warn("Error reading geoip database info");
        }
    }
    return true;
}

void geo_ip_database::close_database() {
    if (m_gi != nullptr) {
        GeoIP_delete(m_gi);
        m_gi = nullptr;
    }
}

std::string geo_ip_database::get_country_code_by_ip(const std::string& ip) const {
    std::string country_code = "--";

    if (m_gi != nullptr) {
        auto logger = spdlog::get(KHL_LOGGER_COMMON);
        auto idx = GeoIP_database_edition(m_gi);
        const auto country_id = GeoIP_id_by_addr(m_gi, ip.c_str());
        country_code = GeoIP_country_code[country_id];

        logger->debug("GetCountryCodeByIP('{}') = '{}'", ip, country_code);
    }

    return country_code;
}
