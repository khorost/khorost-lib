#include "app/config.h"
#include "system/fastfile.h"
#include "util/logger.h"
#include <app/khl-define.h>

using namespace khorost;

bool config::load(const std::string& file_name) {
    Json::Reader reader;
    system::fastfile ff;
    auto result = false;

    if (ff.open_file(file_name, -1, true)) {
        const auto config_text = reinterpret_cast<char*>(ff.get_memory());
        if (reader.parse(config_text, config_text + ff.get_length(), m_container_)) {
            result = true;
        } else {
            auto em = reader.getFormattedErrorMessages();
            const auto logger = spdlog::get(KHL_LOGGER_CONSOLE);
            if (logger != nullptr) {
                logger->warn("Error parse config file - {}", em.c_str());
            }
        }
        ff.close_file();
    }

    return result;
}

config::iterator& config::operator[](const std::string& key) {
    return m_container_[key];
}

bool config::is_value(const std::string& super_key, const bool default_value, const std::string& div) const {
    auto item = find_item(super_key, div);
    return item.isNull() ? default_value : item.asBool();
}

int config::get_value(const std::string& super_key, const int default_value, const std::string& div) const {
    auto item = find_item(super_key, div);
    return item.isNull() ? default_value : item.asInt();
}

std::string config::get_value(const std::string& super_key, const std::string& default_value, const std::string& div) const {
    auto item = find_item(super_key, div);
    return item.isNull() ? default_value : item.asString();
}

config::iterator config::find_item(const std::string& super_key, const std::string& div) const {
    auto item = m_container_;
    auto key = super_key;

    while (true) {
        const auto pos = key.find(div);
        if (std::string::npos != pos) {
            const auto& it2 = item[key.substr(0, pos)];
            if (it2.isNull()) {
                return Json::nullValue;
            }

            item = it2;
            key = key.substr(pos + div.length());
            if (key.empty()) {
                return Json::nullValue;
            }
        } else {
            return item[key];
        }
    }
}
