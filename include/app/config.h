#ifndef _CONFIG__H_
#define _CONFIG__H_

#include <json/json.h>

namespace khorost {
    class Config {
        Json::Value     m_Container;
    public:
        Config() {}
        virtual ~Config(){}

        bool    Load(const std::string& sFileName_);

        typedef Json::Value Iterator;

        Iterator&    operator[](const std::string& sKey_);

        Iterator            FindItem(const std::string& sSuperKey_, const std::string& sDiv_ = ":");
        int                 GetValue(const std::string& sSuperKey_, int nDefaultValue_ = 0, const std::string& sDiv_ = ":");
        const std::string   GetValue(const std::string& sSuperKey_, const std::string& sDefaultValue_ = "", const std::string& sDiv_ = ":");
    private:
    };
};

#endif // _CONFIG__H_

