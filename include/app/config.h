#pragma once

#include <json/json.h>

namespace khorost {
    class config {
        Json::Value m_container_;
    public:
        bool load(const std::string& file_name);

        typedef Json::Value iterator;

        iterator& operator[](const std::string& key);

        iterator find_item(const std::string& super_key, const std::string& div = ":") const;

        bool is_value(const std::string& super_key, bool default_value, const std::string& div = ":") const;
        int get_value(const std::string& super_key, int default_value = 0, const std::string& div = ":") const;
        std::string get_value(const std::string& super_key, const std::string& default_value = "", const std::string& div = ":") const;
    private:
    };
};
