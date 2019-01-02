#ifndef _CONFIG__H_
#define _CONFIG__H_

#include <json/json.h>

namespace khorost {
    class config final {
        Json::Value m_container;
    public:
        config() = default;
        ~config() = default;

        bool load(const std::string& file_name);

        typedef Json::Value iterator;

        iterator& operator[](const std::string& key);

        iterator find_item(const std::string& super_key, const std::string& div = ":") const;
        int get_value(const std::string& super_key, int default_value = 0, const std::string& div = ":") const;
        std::string get_value(const std::string& super_key, const std::string& default_value = "", const std::string& div = ":") const;
    private:
    };
};

#endif // _CONFIG__H_
