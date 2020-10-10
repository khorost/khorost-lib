// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "net/geoip.h"
#include "util/logger.h"
#include <app/khl-define.h>

#ifdef WIN32
#pragma comment(lib, "libmaxminddbd")
#endif // WIN32

using namespace khorost::network;

bool geo_ip_database::open_database(const std::string& path_name) {
    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    const auto status = MMDB_open(path_name.c_str(), MMDB_MODE_MMAP, &m_db_);

    if (status != MMDB_SUCCESS) {
        logger->warn("[GEOIP] Error open geoip database '{}'", path_name);
        return false;
    }

    return true;
}

void geo_ip_database::close_database() {
    MMDB_close(&m_db_);
}

std::string geo_ip_database::get_country_code_by_ip(const std::string& ip) const {
    std::string country_code = "--";

    auto gai_error = 0; // get_address_info() error
    auto db_error = 0;

    auto result = MMDB_lookup_string(const_cast<MMDB_s*const>(&m_db_), ip.c_str(), &gai_error, &db_error);
    if (gai_error) {
    }

    if (db_error) {
    }

    if (result.found_entry) {
        const std::vector<const char*> lookup_path = {"country", "iso_code", nullptr};

        auto entry = &result.entry;
        /*        MMDB_entry_data_list_s* as;
                MMDB_get_entry_data_list(entry, &as);
                MMDB_dump_entry_data_list(stdout, as, 2);
                MMDB_free_entry_data_list(as);
        */
        MMDB_entry_data_s result2;
        auto r = MMDB_aget_value(entry, &result2, lookup_path.data());
        if (result2.has_data) {
            if (result2.type == MMDB_DATA_TYPE_UTF8_STRING) {
                country_code = std::string(result2.utf8_string, result2.data_size);
            }
        }
    }

    return country_code;
}
