#include "app/config.h"
#include "system/fastfile.h"
#include "util/logger.h"

using namespace khorost;

bool Config::Load(const std::string& sFileName_) {
    Json::Reader        reader;
    System::FastFile    ff;
    bool                bResult = false;

    if (ff.Open(sFileName_, -1, true)) {
        char* pConfigText = reinterpret_cast<char*>(ff.GetMemory());
        if (reader.parse(pConfigText, pConfigText + ff.GetLength(), m_Container)) {
            bResult = true;
        } else {
            auto em = reader.getFormattedErrorMessages();
            LOGF(WARNING, "Error parse config file - %s", em.c_str() );
        }
        ff.Close();
    }

    return bResult;
}

Config::Iterator& Config::operator[](const std::string& sKey_) {
    return m_Container[sKey_];
}

int Config::GetValue(const std::string& sSuperKey_, int nDefaultValue_, const std::string& sDiv_) {
    Iterator    item = FindItem(sSuperKey_, sDiv_);
    return item.isNull() ? nDefaultValue_ : item.asInt();
}

const std::string Config::GetValue(const std::string& sSuperKey_, const std::string& sDefaultValue_, const std::string& sDiv_) {
    Iterator    item = FindItem(sSuperKey_, sDiv_);
    return item.isNull() ? sDefaultValue_ : item.asString();
}

Config::Iterator Config::FindItem(const std::string& sSuperKey_, const std::string& sDiv_ ) {
    Config::Iterator    item = m_Container;
    auto sKey = sSuperKey_;

    while (true) {
        auto pos = sKey.find(sDiv_);
        if (std::string::npos != pos) {
            const Config::Iterator&    it2 = item[sKey.substr(0, pos)];
            if (it2.isNull()) {
                return Json::nullValue;
            } else {
                item = it2;
                sKey = sKey.substr(pos + sDiv_.length());
                if (sKey.size() == 0) {
                    return Json::nullValue;
                }
            }
        } else {
            return item[sKey];
        }
    }
}
