#pragma once

#include <map>
#include <memory>
#include <utility>

namespace khorost {
    class internalization final {
    public:
        typedef std::function<std::string(const std::string&, const std::string&)> func_append_value;

        class dictionary final {
            typedef std::map<std::string, std::string> dictionary_map;

            dictionary_map m_map_;
            const std::string m_lang_;
        public:
            explicit dictionary(std::string lang) : m_lang_(std::move(lang)) {
            }

            void clear() {
                m_map_.clear();
            }

            const std::string get_value(const std::string& tag, const func_append_value& func_append_value_if_not_found) {
                const static std::string null_string;
                const auto& it = m_map_.find(tag);
                return it != m_map_.end() ? it->second : (func_append_value_if_not_found != nullptr ? func_append_value_if_not_found(m_lang_, tag) : null_string);
            }

            const std::string& get_value(const std::string& tag) {
                const static std::string null_string;
                const auto& it = m_map_.find(tag);
                return it != m_map_.end() ? it->second : null_string;
            }

            void append_value(const std::string& tag, const std::string& value) {
                m_map_.insert(std::pair<std::string, std::string>(tag, value));
            }
        };

        typedef std::shared_ptr<dictionary> dictionary_ptr;
        typedef std::map<std::string, dictionary_ptr> library;

    protected:
        library m_library_;
    public:
        internalization() = default;

        dictionary_ptr get_dictionary(const std::string& lang) {
            const auto it = m_library_.find(lang);
            if (it != m_library_.end()) {
                return it->second;
            }
            return append_dictionary(lang);
        }

        void clear() {
            m_library_.clear();
        }

        const std::string& get_value(const std::string& lang, const std::string& tag) {
            const static std::string null_string;
            const auto& dict = get_dictionary(lang);
            return dict != nullptr ? dict->get_value(tag) : null_string;
        }

        void append_value(const std::string& lang, const std::string& tag, const std::string& value) {
            const auto& language_dictionary = get_dictionary(lang);
            language_dictionary->append_value(tag, value);
        }

        dictionary_ptr append_dictionary(const std::string& lang) {
            auto language_dictionary = std::make_shared<dictionary>(lang);
            m_library_.insert(std::pair<std::string, dictionary_ptr>(lang, language_dictionary));
            return language_dictionary;
        }
    };
}
