// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#ifndef NOMINMAX
 #define NOMINMAX
#endif

#include <string>
#include <algorithm>
#include <time.h>

#include "net/http.h"
#include "system/fastfile.h"
#include "util/utils.h"
#include "app/khl-define.h"

#ifdef WIN32
#include "shlwapi.h"
#include "win/strptime.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#define MAX_PATH PATH_MAX
#endif

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"libcurl.lib")

using namespace khorost;
using namespace khorost::network;

bool find_sub_value(const char* pSource_, size_t nSourceSize_, const char* pMatch_, size_t nMatchSize_, char cDivKV,
                    char cDivKK, const char** pResult_ = nullptr, size_t* pnResultSize_ = nullptr) {
    if (pSource_ == nullptr) {
        return false;
    }

    if (nSourceSize_ == data::auto_buffer_char::npos) {
        nSourceSize_ = strlen(pSource_);
    }

    if (nSourceSize_ < nMatchSize_) {
        return false;
    }

    auto pMax = nSourceSize_ - nMatchSize_;
    bool bInQuote = false;
    for (size_t pStart = 0, pCheck = 0; pCheck <= pMax; ++pCheck) {
        if (pSource_[pCheck] == '\"') {
            bInQuote = !bInQuote;
        } else if (!bInQuote && memcmp(pSource_ + pCheck, pMatch_, nMatchSize_) == 0) {
            if (pResult_ != nullptr && pnResultSize_ != nullptr) {
                *pResult_ = nullptr;
                *pnResultSize_ = 0;

                for (pCheck += nMatchSize_, pMax = nSourceSize_; pCheck < pMax; ++pCheck) {
                    if (pSource_[pCheck] == '\"') {
                        bInQuote = !bInQuote;
                    } else if (!bInQuote) {
                        if (pSource_[pCheck] == cDivKV) {
                            pStart = pCheck + sizeof(char);
                            *pResult_ = pSource_ + pStart;
                        } else if (pSource_[pCheck] == cDivKK) {
                            --pCheck;
                            break;
                        }
                    }
                }

                if (*pResult_ != nullptr) {
                    if (pCheck == pMax) {
                        --pCheck;
                    }
                    // значение имеется, его нужно проверить
                    for (; *(*pResult_) == ' '; ++(*pResult_), ++pStart) {
                    }
                    if (*(*pResult_) == '\"') {
                        ++(*pResult_);
                        ++pStart;
                    }
                    for (; *(pSource_ + pCheck) == ' '; --pCheck) {
                    }
                    if (*(pSource_ + pCheck) == '\"') {
                        --pCheck;
                    }
                    *pnResultSize_ = pCheck - pStart + 1;
                }
            }

            return true;
        }
    }

    return false;
}

void http_text_protocol_header::response::set_header_param(const std::string& key, const std::string& value) {
    m_header_params_.insert(std::pair<std::string, std::string>(key, value));
}

void http_text_protocol_header::response::send_header_data(connection& connect) {
    for (const auto& it : m_header_params_) {
        connect.send_string(it.first);
        connect.send_string(": ");
        connect.send_string(it.second);
        connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    }
}

bool http_text_protocol_header::get_chunk(const char*& rpBuffer_, size_t& rnBufferSize_, const char prefix,
                                          const char* div, data::auto_buffer_char& abTarget_,
                                          data::auto_buffer_chunk_char& rabcQueryValue_, size_t& rnChunkSize_) {
    size_t s;
    // зачишаем префикс от white символов
    for (s = 0; s < rnBufferSize_ && rpBuffer_[s] == prefix; ++s) {
    }

    for (auto k = s; k < rnBufferSize_; ++k) {
        // выделяем chunk до stop символов
        for (size_t t = 0; div[t] != '\0'; ++t) {
            if (rpBuffer_[k] == div[t]) {
                // stop символ найден, chunk полный
                t = abTarget_.get_fill_size();
                rnChunkSize_ = k + sizeof(char);
                abTarget_.append(rpBuffer_, rnChunkSize_);
                abTarget_[t + k] = '\0'; // заменяем stop символ на завершение zстроки
                rabcQueryValue_.set_reference(t + s);

                rpBuffer_ += rnChunkSize_;
                rnBufferSize_ -= rnChunkSize_;

                return true;
            }
        }
    }
    return false;
}

size_t http_text_protocol_header::process_data(const uint8_t* buffer, size_t buffer_size) {
    size_t process_byte = 0, chunk_size, chunk_size_v;
    switch (m_eHeaderProcess) {
    case eProcessingFirst:
        //  GET / POST ****************************************************
        if (!m_query_method_.is_valid()) {
            if (!get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, ' ', " ", m_ab_header_, m_query_method_,
                           chunk_size)) {
                return process_byte;
            }
            process_byte += chunk_size;
        }
        //  /index.html ***************************************************
        if (!m_query_uri_.is_valid()) {
            if (!get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, ' ', "? ", m_ab_header_, m_query_uri_,
                           chunk_size)) {
                return process_byte;
            }
            if (*(buffer - sizeof(char)) == '?') {
                --chunk_size;
                buffer -= sizeof(char);
                buffer_size += sizeof(char);
            }
            process_byte += chunk_size;
        }
        //  ?key=val& ***************************************************
        if (m_ab_params_.get_fill_size() == 0 && buffer[0] == '?') {
            data::auto_buffer_chunk_char buffer_param(m_ab_params_);
            buffer += sizeof(char);
            buffer_size -= sizeof(char);
            if (!get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, '?', " ", m_ab_params_, buffer_param,
                           chunk_size)) {
                return process_byte;
            }
            process_byte += chunk_size + sizeof(char);
        }
        //  HTTP/1.1   ****************************************************
        if (!m_query_version_.is_valid()) {
            if (!get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, ' ', "\r\n", m_ab_header_,
                           m_query_version_, chunk_size)) {
                return process_byte;
            }
            if (0 < buffer_size && (buffer[0] == '\r' || buffer[0] == '\n')) {
                ++chunk_size;
                buffer += sizeof(char);
                buffer_size -= sizeof(char);
            }
            process_byte += chunk_size;

            m_eHeaderProcess = eProcessingNext;
        }
    case eProcessingNext:
        while (buffer_size >= 2 * sizeof(char)) {
            if (buffer[0] == '\r' && buffer[1] == '\n') {
                m_eHeaderProcess = eSuccessful;
                m_eBodyProcess = eProcessingFirst;

                buffer += 2 * sizeof(char);
                buffer_size -= 2 * sizeof(char);
                process_byte += 2 * sizeof(char);
                break;
            }

            data::auto_buffer_chunk_char header_key(m_ab_header_);
            data::auto_buffer_chunk_char header_value(m_ab_header_);

            if (get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, ' ', ":", m_ab_header_, header_key,
                          chunk_size)
                && get_chunk(reinterpret_cast<const char*&>(buffer), buffer_size, ' ', "\r\n", m_ab_header_,
                             header_value, chunk_size_v)) {
                if (0 < buffer_size && (buffer[0] == '\r' || buffer[0] == '\n')) {
                    ++chunk_size;
                    buffer += sizeof(char);
                    buffer_size -= sizeof(char);
                }
                process_byte += chunk_size + chunk_size_v;

                m_header_values_.push_back(std::make_pair(header_key.get_reference(), header_value.get_reference()));
                if (m_request_.m_content_length == static_cast<size_t>(-1) && strcmp(
                    HTTP_ATTRIBUTE_CONTENT_LENGTH, header_key.get_chunk()) == 0) {
                    m_request_.m_content_length = atoi(header_value.get_chunk());
                } else if (strcmp(HTTP_ATTRIBUTE_COOKIE, header_key.get_chunk()) == 0) {
                    parse_string(const_cast<char*>(header_value.get_chunk()), chunk_size_v,
                                 header_value.get_reference(), m_cookies_, ';', true);
                }
            } else {
                m_ab_header_.decrement_free_size(chunk_size);
                break;
            }
        }
        break;
    }

    if (m_eBodyProcess == eProcessingFirst || m_eBodyProcess == eProcessingNext) {
        chunk_size = std::min(buffer_size, m_request_.m_content_length);
        if (m_request_.m_content_length == std::string::npos || (buffer_size == 0 && m_request_.m_content_length == 0)) {
            m_eBodyProcess = eSuccessful;
            m_request_.m_content_length = 0;
        } else if (chunk_size != 0) {
            m_ab_body_.append(reinterpret_cast<const char*>(buffer), chunk_size);
            process_byte += chunk_size;

            if (m_ab_body_.get_fill_size() >= m_request_.m_content_length) {
                m_eBodyProcess = eSuccessful;

                const auto content_type = get_header_parameter(HTTP_ATTRIBUTE_CONTENT_TYPE, nullptr);
                if (content_type != nullptr && find_sub_value(content_type, data::auto_buffer_char::npos,
                                                              HTTP_ATTRIBUTE_CONTENT_TYPE__FORM,
                                                              sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__FORM) - 1, '=',
                                                              ';')) {
                    //                if (strcmp(HTTP_ATTRIBUTE_CONTENT_TYPE__FORM, pszContentType) == 0) {
                    const auto k = m_ab_params_.get_fill_size();
                    if (k != 0) {
                        if (m_ab_params_.get_element(k - 1) == '\0') {
                            if (k > 1 && m_ab_params_.get_element(k - 2) != '&') {
                                m_ab_params_[k - 1] = '&';
                            } else {
                                m_ab_params_.increment_free_size(sizeof(char));
                            }
                        } else if (m_ab_params_.get_element(k - 1) != '&') {
                            m_ab_params_.append("&", sizeof(char));
                        }
                        m_request_.m_content_length += static_cast<int>(m_ab_params_.get_fill_size());
                    }

                    m_ab_params_.append(m_ab_body_.get_head(), m_ab_body_.get_fill_size());
                    m_ab_params_.append("\0", sizeof(char));
                }
            }
        }
        if (m_eBodyProcess == eSuccessful) {
            parse_string(m_ab_params_.get_position(0), m_ab_params_.get_fill_size(), 0, m_params_value_, '&', false);
        }
    }

    return process_byte;
}

bool http_text_protocol_header::get_multi_part(size_t& current_iterator, std::string& part_name,
                                               std::string& part_content_type, const char*& buffer_content,
                                               size_t& buffer_content_size) {
    const auto header_content_type = get_header_parameter(HTTP_ATTRIBUTE_CONTENT_TYPE, nullptr);
    if (header_content_type == nullptr) {
        return false;
    }

    const char* pszBoundary = nullptr;
    const auto header_content_type_length = strlen(header_content_type);
    size_t size_boundary = 0;

    if (find_sub_value(header_content_type, header_content_type_length,
                       HTTP_ATTRIBUTE_CONTENT_TYPE__MULTIPART_FORM_DATA,
                       sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__MULTIPART_FORM_DATA) - 1, '=', ';')) {
        if (find_sub_value(header_content_type, header_content_type_length, HTTP_ATTRIBUTE_CONTENT_TYPE__BOUNDARY,
                           sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__BOUNDARY) - 1, '=', ';', &pszBoundary, &size_boundary)) {
            std::string boundary_label = HTTP_ATTRIBUTE_LABEL_CD;
            boundary_label.append(pszBoundary, size_boundary);
            // проверить что контрольная метка правильная
            if (m_ab_body_.compare(current_iterator, boundary_label.c_str(), boundary_label.size()) != 0) {
                return false;
            }
            current_iterator += boundary_label.size();

            if (m_ab_body_.compare(current_iterator, HTTP_ATTRIBUTE_LABEL_CD, sizeof(HTTP_ATTRIBUTE_LABEL_CD) - 1) == 0) {
                return false; // завершение блока
            }

            if (m_ab_body_.compare(current_iterator, HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1) != 0) {
                return false;
            }
            current_iterator += sizeof(HTTP_ATTRIBUTE_ENDL) - 1;

            const auto p_max = m_ab_body_.get_fill_size();
            while (current_iterator < p_max) {
                auto p_end_line = m_ab_body_.find(current_iterator, HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
                if (p_end_line == data::auto_buffer_char::npos) {
                    p_end_line = m_ab_body_.get_fill_size() - current_iterator;
                }

                if (p_end_line == current_iterator) {
                    current_iterator += sizeof(HTTP_ATTRIBUTE_ENDL) - 1;
                    boundary_label.insert(0, HTTP_ATTRIBUTE_ENDL);
                    p_end_line = m_ab_body_.find(current_iterator, boundary_label.c_str(), boundary_label.size());
                    if (p_end_line == data::auto_buffer_char::npos) {
                        return false;
                    }

                    buffer_content = m_ab_body_.get_head() + current_iterator;
                    buffer_content_size = p_end_line - current_iterator;
                    current_iterator = p_end_line + sizeof(HTTP_ATTRIBUTE_ENDL) - 1;

                    return true;
                }

                const char* value;
                size_t value_size;
                if (m_ab_body_.compare(current_iterator, HTTP_ATTRIBUTE_CONTENT_DISPOSITION,
                                       sizeof(HTTP_ATTRIBUTE_CONTENT_DISPOSITION) - 1) == 0) {
                    if (find_sub_value(m_ab_body_.get_head() + current_iterator, p_end_line - current_iterator, "name",
                                       sizeof("name") - 1, '=', ';', &value, &value_size)) {
                        part_name.assign(value, value_size);
                    }
                } else if (m_ab_body_.compare(current_iterator, HTTP_ATTRIBUTE_CONTENT_TYPE,
                                              sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE) - 1) == 0) {
                    if (find_sub_value(m_ab_body_.get_head() + current_iterator, p_end_line - current_iterator,
                                       HTTP_ATTRIBUTE_CONTENT_TYPE, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE) - 1, ':', ';',
                                       &value, &value_size)) {
                        part_content_type.assign(value, value_size);
                    }
                }
                current_iterator = p_end_line + sizeof(HTTP_ATTRIBUTE_ENDL) - 1;
            }
        }
    }

    // Content-Type: multipart/form-data; boundary=xmyidlatfdmcrqnk; charset=UTF-8
    // Content-Disposition: form-data; name="image"; filename="1.png"
    //    Content - Type: image / png


    return false;
}

void http_text_protocol_header::parse_string(char* buffer, const size_t buffer_size, const size_t shift, list_pairs& target, const char div,
                                             const bool trim) {
    size_t k, v, t;
    for (k = 0, v = -1, t = 0; t < buffer_size;) {
        if (buffer[t] == '\0') {
            if (trim) {
                for (; buffer[k] == ' '; ++k) {
                }
            }
            target.push_back(std::make_pair(shift + k, v != -1 ? shift + v : v));
            break;
        }

        if (buffer[t] == div) {
            buffer[t] = '\0';
            if (trim) {
                for (; buffer[k] == ' '; ++k) {
                }
            }
            target.push_back(std::make_pair(shift + k, v != -1 ? shift + v : v));
            k = ++t;
            v = -1;
        } else if (buffer[t] == '=') {
            buffer[t] = '\0';
            v = ++t;
        } else {
            ++t;
        }
    }
}

const char* http_text_protocol_header::get_cookie_parameter(const std::string& key, const char* default_value) const {
    const auto* value = get_cookie(key, nullptr);
    if (value == nullptr) {
        value = get_parameter(key, default_value);
    }
    return value;
}

const char* http_text_protocol_header::get_cookie(const std::string& key, const char* default_value) const {
    for (const auto& cit : m_cookies_) {
        if (strcmp(key.c_str(), m_ab_header_.get_position(cit.first)) == 0) {
            return cit.second != std::string::npos ? m_ab_header_.get_position(cit.second) : default_value;
        }
    }
    return default_value;
}

bool http_text_protocol_header::is_parameter_exist(const std::string& key) const {
    for (const auto& cit : m_params_value_) {
        if (strcmp(key.c_str(), m_ab_params_.get_position(cit.first)) == 0) {
            return true;
        }
    }
    return false;
}

size_t http_text_protocol_header::get_parameter_index(const std::string& key) const {
    for (const auto& cit : m_params_value_) {
        if (strcmp(key.c_str(), m_ab_params_.get_position(cit.first)) == 0) {
            return cit.second;
        }
    }
    return -1;
}

size_t http_text_protocol_header::get_header_index(const std::string& key) const {
    for (const auto& cit : m_header_values_) {
        if (strcmp(key.c_str(), m_ab_header_.get_position(cit.first)) == 0) {
            return cit.second;
        }
    }
    return -1;
}

void http_text_protocol_header::fill_parameter2_array(const std::string& s_key, std::vector<int>& r_array) {
    const auto s_key2 = s_key + "[]";
    for (const auto& p : m_params_value_) {
        if (strcmp(s_key.c_str(), m_ab_params_.get_position(p.first)) == 0 || strcmp(
            s_key2.c_str(), m_ab_params_.get_position(p.first)) == 0) {
            r_array.push_back(atoi(m_ab_params_.get_position(p.second)));
        }
    }
}

const char* http_text_protocol_header::get_parameter(const std::string& key, const char* default_value) const {
    for (const auto& cit : m_params_value_) {
        if (strcmp(key.c_str(), m_ab_params_.get_position(cit.first)) == 0) {
            return cit.second != std::string::npos ? m_ab_params_.get_position(cit.second) : default_value;
        }
    }
    return default_value;
}

const char* http_text_protocol_header::get_header_parameter(const std::string& param, const char* default_value) const {
    for (const auto& cit : m_header_values_) {
        if (strcmp(param.c_str(), m_ab_header_.get_position(cit.first)) == 0) {
            return m_ab_header_.get_position(cit.second);
        }
    }

    return default_value;
}

bool http_text_protocol_header::is_send_data() const {
    return strcmp(get_query_method(), "HEAD") != 0;
}

void http_text_protocol_header::send_response(network::connection& connect, const char* response, const size_t length) {
    using namespace boost::posix_time;

    char st[255];

    m_response_.m_content_length = length;

    const auto http_version = m_query_version_.get_chunk();

    connect.send_string(http_version);
    connect.send_string(" ", sizeof(char));
    connect.send_number(m_response_.m_code);
    connect.send_string(" ", sizeof(char));
    connect.send_string(m_response_.m_code_reason);
    connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    connect.send_string("Server: phreeber" HTTP_ATTRIBUTE_ENDL);

    auto n = time(nullptr);
    strftime(st, sizeof(st), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&n));
    connect.send_string("Date: ");
    connect.send_string(st);
    connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);

    if (!m_response_.m_redirect_url.empty()) {
        connect.send_string("Location: ");
        connect.send_string(m_response_.m_redirect_url);
        connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    }

    const auto connection_keep_alive = get_header_parameter(
        HTTP_ATTRIBUTE_CONNECTION, strcmp(http_version, HTTP_VERSION_1_1) == 0
                                       ? HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE
                                       : HTTP_ATTRIBUTE_CONNECTION__CLOSE);
    m_response_.m_auto_close = strcmp(HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE, connection_keep_alive) != 0;

    connect.send_string(
        std::string("Connection: ") + std::string(connection_keep_alive) + std::string(HTTP_ATTRIBUTE_ENDL));

    // CORS
    /*    const auto origin = get_header_parameter(HTTP_ATTRIBUTE__ORIGIN, nullptr);
        if (origin != nullptr) {
            connect.send_string(HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_ORIGIN ": " + std::string(origin) + std::string(HTTP_ATTRIBUTE_ENDL));
            connect.send_string(HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_CREDENTIALS ": true" HTTP_ATTRIBUTE_ENDL);
        }
    */
    for (const auto& c : m_response_.m_cookies) {
        time_t gmt = data::epoch_diff(c.m_expire).total_seconds();
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&gmt));

        connect.send_string(
            std::string("Set-Cookie: ") + c.m_name_cookie + std::string("=") + c.m_value + std::string("; Expires=") +
            std::string(st) + std::string("; Domain=.") + c.m_domain + std::string("; Path=/") +
            (c.m_http_only ? std::string("; HttpOnly ") : "") + HTTP_ATTRIBUTE_ENDL);
    }

    connect.send_string(std::string(HTTP_ATTRIBUTE_CONTENT_TYPE HTTP_ATTRIBUTE_DIV) + m_response_.m_content_type);
    if (!m_response_.m_content_type_codepage.empty()) {
        connect.send_string(std::string("; charset=") + m_response_.m_content_type_codepage);
    }
    connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);

    if (!m_response_.m_content_disposition.empty()) {
        connect.send_string(
            std::string(HTTP_ATTRIBUTE_CONTENT_DISPOSITION HTTP_ATTRIBUTE_DIV) + m_response_.m_content_disposition);
        connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    }

    connect.send_string(HTTP_ATTRIBUTE_CONTENT_LENGTH ": ");
    connect.send_number(static_cast<unsigned int>(m_response_.m_content_length));
    connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);

    if (!m_response_.m_last_modify.is_not_a_date_time()) {
        connect.send_string(HTTP_ATTRIBUTE_LAST_MODIFIED HTTP_ATTRIBUTE_DIV);
        time_t gmt = data::epoch_diff(m_response_.m_last_modify).total_seconds();
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&gmt));
        connect.send_string(st);
        connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    }

    m_response_.send_header_data(connect);

    connect.send_string(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    if (is_send_data() && response != nullptr) {
        connect.send_string(response, m_response_.m_content_length);
    }
}

void http_text_protocol_header::set_cookie(const std::string& cookie, const std::string& value,
                                           boost::posix_time::ptime expire, const std::string& domain, bool http_only) {
    m_response_.m_cookies.emplace_back(cookie, value, expire, domain, http_only);
}

#ifdef WIN32
#define DIRECTORY_DIV   '\\'
static bool IsSlash(char ch_) { return ch_ == '/'; }
#else
#define DIRECTORY_DIV   '/'
#endif


bool http_text_protocol_header::send_file(const std::string& query_uri, network::connection& connect,
                                          const std::string& doc_root, const std::string& file_name) {
    using namespace boost::posix_time;

    auto logger = spdlog::get(KHL_LOGGER_COMMON);

    // TODO сделать корректировку по абсолютно-относительным переходам
    std::string s_file_name;
    if (file_name.empty()) {
        s_file_name = doc_root + query_uri;
    } else {
        s_file_name = doc_root + file_name;
    }

#ifdef WIN32
    std::replace_if(s_file_name.begin(), s_file_name.end(), IsSlash, DIRECTORY_DIV);

    const auto dw_attr = GetFileAttributes(s_file_name.c_str());
#else
    struct stat s;
#endif

    if (*s_file_name.rbegin() == DIRECTORY_DIV) {
        s_file_name += "index.html";
#ifdef WIN32
    } else if (dw_attr != static_cast<DWORD>(-1) && dw_attr & FILE_ATTRIBUTE_DIRECTORY) {
#else
    } else if (stat(s_file_name.c_str(), &s)!=-1 && S_ISDIR(s.st_mode)) {
#endif
        s_file_name.append(1, DIRECTORY_DIV);
        s_file_name += "index.html";
    }

    std::string s_canonic_file_name;

    if (s_file_name.length() >= MAX_PATH) {
        logger->warn("Path is too long");

        set_response_status(http_response_status_not_found, "Not found");
        send_response(connect, "File not found");
        return false;
    }

    char buf_canonic_file_name[MAX_PATH];
    memset(buf_canonic_file_name, 0, sizeof(buf_canonic_file_name));

#ifdef WIN32
    if (!PathCanonicalize(static_cast<LPSTR>(buf_canonic_file_name), s_file_name.c_str())) {
        logger->warn("PathCanonicalize failed");

        set_response_status(http_response_status_not_found, "Not found");
        send_response(connect, "File not found");
        return false;
    }
#else
    if (!realpath(s_file_name.c_str(), buf_canonic_file_name)) {
        logger->warn("realpath failed");

        set_response_status(http_response_status_not_found, "Not found");
        send_response(connect, "File not found");
        return false;
    }
#endif

    s_canonic_file_name = buf_canonic_file_name;

    if (s_canonic_file_name.substr(0, doc_root.length()) != doc_root) {
        logger->warn("Access outside of docroot attempted");

        set_response_status(http_response_status_not_found, "Not found");
        send_response(connect, "File not found");
        return false;
    }

    system::fastfile ff;
    if (ff.open_file(s_canonic_file_name, -1, true)) {
        static struct sect {
            const char* m_ext;
            const char* m_ct;
            const char* m_cp;
        } s_sect[] = {
                {"js", HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS, HTTP_CODEPAGE_UTF8},
                {"html", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8},
                {"htm", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8},
                {"css", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_CSS, HTTP_CODEPAGE_UTF8},
                {"gif", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_GIF, HTTP_CODEPAGE_NULL},
                {"jpg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL},
                {"jpeg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL},
                {"png", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_PNG, HTTP_CODEPAGE_NULL},
                {nullptr, nullptr, nullptr}
            };

        const char* ims = get_header_parameter("If-Modified-Since", nullptr);
        if (ims != nullptr) {
            tm t;
            (ims, "%a, %d-%b-%Y %H:%M:%S GMT", &t);
#ifdef WIN32
            time_t tt = _mkgmtime(&t);
#else
            time_t tt = timegm(&t);
#endif  // WIN32
            if (tt >= ff.get_time_update()) {
                logger->debug("[HTTP] Don't send file '{}' length = {:d}. Response 304 (If-Modified-Since: '{}')",
                              query_uri.c_str(), ff.get_length(), ims);

                set_response_status(304, "Not Modified");
                send_response(connect, nullptr, 0);
                return true;
            }
        }

        auto p_ext = s_canonic_file_name.c_str();
        auto n_ext = s_canonic_file_name.size();
        for (; n_ext > 0; --n_ext) {
            if (p_ext[n_ext] == '.') {
                n_ext++;
                break;
            }

            if (p_ext[n_ext] == '\\' || p_ext[n_ext] == '/') {
                n_ext = 0;
                break;
            }
        }

        logger->debug("[HTTP] Send file '{}' length = {:d}", query_uri.c_str(), ff.get_length());

        if (n_ext > 0) {
            p_ext += n_ext;
            for (auto k = 0; s_sect[k].m_ext != nullptr; ++k) {
                if (strcmp(s_sect[k].m_ext, p_ext) == 0) {
                    set_content_type(s_sect[k].m_ct, s_sect[k].m_cp);
                    break;
                }
            }
        }

        set_last_modify(from_time_t(ff.get_time_update()));
        send_response(connect, nullptr, ff.get_length());

        if (is_send_data()) {
            // TODO добавить вариант отправки чанками
            connect.send_data(static_cast<const uint8_t*>(ff.get_memory()), ff.get_length());
        }

        ff.close_file();
    } else {
        logger->warn("[HTTP] File not found '{}'", s_canonic_file_name.c_str());

        set_response_status(http_response_status_not_found, "Not found");
        send_response(connect, "File not found");
        return false;
    }

    return true;
}

bool http_text_protocol_header::get_parameter(const std::string& key, const bool default_value) const {
    const auto value = get_parameter(key, nullptr);
    return value != nullptr ? strcmp(value, "true") == 0 : default_value;
}

int http_text_protocol_header::get_parameter(const std::string& key, const int default_value) const {
    const auto value = get_parameter(key, nullptr);
    return value != nullptr ? atoi(value) : default_value;
}

int64_t http_text_protocol_header::get_parameter64(const std::string& key, const int64_t default_value) const {
    const auto value = get_parameter(key, nullptr);
    return value != nullptr ? atoll(value) : default_value;
}

void http_text_protocol_header::calculate_host_port() {
    auto idx = get_header_index(HTTP_ATTRIBUTE_HOST);
    if (idx == std::string::npos) {
        return;
    }

    m_host_ = idx;

    for (;; ++idx) {
        const auto ch = *m_ab_header_.get_position(idx);

        if (ch == '\0') {
            m_port_ = 0;
            break;
        }

        if (ch == ':') {
            m_ab_header_[idx] = '\0';
            m_port_ = idx + 1;
            break;
        }
    }
}

const char* http_text_protocol_header::get_host() {
    if (m_host_ == std::string::npos) {
        calculate_host_port();
    }
    return m_host_ == std::string::npos ? "" : m_ab_header_.get_position(m_host_);
}

const char* http_text_protocol_header::get_port() {
    if (m_host_ == std::string::npos) {
        calculate_host_port();
    }
    return (m_port_ == std::string::npos || m_port_ == 0) ? "" : m_ab_header_.get_position(m_port_);
}

const char* http_text_protocol_header::get_client_proxy_ip() const {
    auto idx = get_header_index(HTTP_ATTRIBUTE_X_FORWARDED_FOR);
    if (idx == std::string::npos) {
        idx = get_header_index(HTTP_ATTRIBUTE_X_REAL_IP);
        if (idx == std::string::npos) {
            return nullptr;
        }
    }
    return m_ab_header_.get_position(idx);
}

static size_t sWriteAutobufferCallback(void* Contents_, size_t nSize_, size_t nMemb_, void* Ctx_) {
    size_t nRealSize = (nSize_ * nMemb_) / sizeof(char);
    data::auto_buffer_t<char>* pBuffer = reinterpret_cast<data::auto_buffer_t<char>*>(Ctx_);

    pBuffer->check_size(pBuffer->get_fill_size() + nRealSize);
    pBuffer->append(reinterpret_cast<const char*>(Contents_), nRealSize);

    return nRealSize * sizeof(char);
}

std::string http_curl_string::do_post_request(const std::string& uri, const std::string& request) const {
    data::auto_buffer_char response;
    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(headers, "Content-Type:application/json");
    CURL* curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L); /* allow connections to be reused */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "rewtas agent");
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60 * 60L); /* timeout of 60 minutes */
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&response));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sWriteAutobufferCallback);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.empty() ? 0 : request.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.empty() ? nullptr : request.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    CURLcode response_code, ret;
    ret = curl_easy_perform(curl);
    const auto err = curl_easy_strerror(ret);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    std::string body;
    if (response.get_fill_size() != 0) {
        body.assign(response.get_head(), response.get_fill_size());
    }
    return std::move(body);
}
