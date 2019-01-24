#pragma once

#include <map>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <WinSock2.h>
#else
/* For sockaddr_in */
# include <netinet/in.h>
/* For socket functions */
# include <sys/socket.h>
# include <unistd.h>
#endif  

#include <curl/curl.h>
#include "boost/date_time/posix_time/posix_time.hpp"

#include "util/autobuffer.h"
#include "util/logger.h"
#include "net/connection.h"

#define HTTP_ATTRIBUTE_AUTHORIZATION        "Authorization"
#define HTTP_ATTRIBUTE_CONTENT_LENGTH       "Content-Length"
#define HTTP_ATTRIBUTE_CONTENT_TYPE         "Content-Type"
#define HTTP_ATTRIBUTE_CONTENT_DISPOSITION  "Content-Disposition"
#define HTTP_ATTRIBUTE_HOST                 "Host"
#define HTTP_ATTRIBUTE_CONNECTION           "Connection"
#define HTTP_ATTRIBUTE_TRANSFER_ENCODING    "Transfer-Encoding"
#define HTTP_ATTRIBUTE_CONTENT_ENCODING     "Content-Encoding"
#define HTTP_ATTRIBUTE_COOKIE               "Cookie"
#define HTTP_ATTRIBUTE_LAST_MODIFIED        "Last-Modified"
#define HTTP_ATTRIBUTE_X_REAL_IP            "X-Real-IP"
#define HTTP_ATTRIBUTE_X_FORWARDED_FOR      "X-Forwarded-For"
#define HTTP_ATTRIBUTE_ACCEPT_LANGUAGE      "Accept-Language"
#define HTTP_ATTRIBUTE_USER_AGENT           "User-Agent"
#define HTTP_ATTRIBUTE_ACCEPT_ENCODING      "Accept-Encoding"
#define HTTP_ATTRIBUTE_ACCEPT               "Accept"
#define HTTP_ATTRIBUTE_REFERER              "Referer"
#define HTTP_ATTRIBUTE__ORIGIN                              "Origin"
#define HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_ORIGIN         "Access-Control-Allow-Origin"
#define HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_CREDENTIALS    "Access-Control-Allow-Credentials"

#define HTTP_LANGUAGE_RU                    "ru"
#define HTTP_LANGUAGE_EN                    "en"
#define HTTP_LANGUAGE_DE                    "de"
#define HTTP_LANGUAGE_FR                    "fr"

#define HTTP_ATTRIBUTE_DIV                  ": "
#define HTTP_ATTRIBUTE_ENDL                 "\r\n"
#define HTTP_ATTRIBUTE_LABEL_CD             "--"

#define HTTP_ATTRIBUTE_CONTENT_TYPE__FORM       "application/x-www-form-urlencoded"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML  "text/html"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_CSS   "text/css"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_JS    "text/javascript"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_CSV   "test/csv"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS     "application/javascript"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JSON   "application/json"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__APP_EXCEL  "application/vnd.ms-excel"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_GIF  "image/gif"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_PNG  "image/png"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG  "image/jpeg"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__MULTIPART_FORM_DATA       "multipart/form-data"
#define HTTP_ATTRIBUTE_CONTENT_TYPE__BOUNDARY   "boundary"

#define HTTP_VERSION_1_0                        "HTTP/1.0"
#define HTTP_VERSION_1_1                        "HTTP/1.1"

#define HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE   "keep-alive"
#define HTTP_ATTRIBUTE_CONNECTION__CLOSE        "close"

#define HTTP_CODEPAGE_UTF8                      "UTF-8"
#define HTTP_CODEPAGE_NULL                      ""

#define HTTP_RESPONSE_STATUS_OK                 200
#define HTTP_RESPONSE_STATUS_UNAUTHORIZED       401
#define HTTP_RESPONSE_STATUS_NOT_FOUND          404

#define HTTP_RESPONSE_STATUS_ERROR              503

#define HTTP_RESPONSE_STATUS_UNAUTHORIZED_DESC  "Unauthorized"
#define HTTP_RESPONSE_STATUS_ERROR_DESC         "Server internal error"

namespace khorost {
    namespace network {
        class http_text_protocol_header {
            // Быстрый доступ к атрибутам HTTP
            data::AutoBufferChunkChar   m_query_method_;
            data::AutoBufferChunkChar   m_query_uri_;
            data::AutoBufferChunkChar   m_query_version_;

            size_t  m_nHost{};
            size_t  m_nPort{};

            typedef std::list< std::pair<size_t, size_t> > ListPairs;

            ListPairs   m_header_values;  // словарь для заголовка
            ListPairs   m_params_value_;  // словарь для параметров
            ListPairs   m_cookies_;      // словарь для кук

            data::AutoBufferChar                    m_abHeader;
            data::AutoBufferChar                    m_abParams;
            data::AutoBufferChar                    m_abBody;
            enum {
                eNone
                , eProcessingFirst
                , eProcessingNext
                , eSuccessful
                , eError
            }   m_eHeaderProcess, m_eBodyProcess;

            struct request final {
                size_t    m_content_length_;

                void clear() {
                    m_content_length_ = -1;
                }
            } m_request_;

            // ****************************************************************
            class response final {
                std::map<std::string, std::string> m_header_params_;
            public:
                struct cookie final {
                    std::string m_sCookie;
                    std::string m_sValue;
                    boost::posix_time::ptime m_dtExpire;
                    std::string m_sDomain;
                    bool m_http_only = false;

                    cookie() = default;
                    ~cookie() = default;

                    cookie(const cookie& right) {
                        if (this != &right) {
                            *this = right;
                        }
                    }
                    cookie(const std::string& sCookie_, const std::string& sValue_, boost::posix_time::ptime dtExpire_, const std::string& sDomain_, const bool http_only) {
                        m_sCookie = sCookie_;
                        m_sValue = sValue_;
                        m_dtExpire = dtExpire_;
                        m_sDomain = sDomain_;
                        m_http_only = http_only;
                    }
                    cookie& operator=(const cookie& right) {
                        if (this != &right) {
                            m_sCookie = right.m_sCookie;
                            m_sValue = right.m_sValue;
                            m_dtExpire = right.m_dtExpire;
                            m_sDomain = right.m_sDomain;
                            m_http_only = right.m_http_only;
                        }
                        return *this;
                    }
                };
                std::deque<cookie>  m_cookies_;
                bool                m_auto_close;
                int                 m_nCode;
                std::string         m_sCodeReason;
                std::string         m_sContentType;
                std::string         m_sContentTypeCP;
                boost::posix_time::ptime              m_tLastModify;
                std::string         m_sContentDisposition;
                std::string         m_sRedirectURL;
                size_t m_content_length_;

                void    clear() {
                    m_cookies_.clear();
                    m_auto_close = true;
                    m_nCode = HTTP_RESPONSE_STATUS_OK;
                    m_sCodeReason = "Ok";
                    m_sContentType = HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML;
                    m_sContentTypeCP = "UTF-8";
                    m_sContentDisposition = "";
                    m_tLastModify = boost::posix_time::ptime(boost::posix_time::not_a_date_time);
                    m_content_length_ = 0;

                    m_header_params_.clear();
                }

                void set_header_param(const std::string& key, const std::string& value) ;
                void send_header_data(connection& connect);

            }   m_response_;

            bool    get_chunk(const char*& rpBuffer_, size_t& rnBufferSize_, char cPrefix_, const char* pDiv_, data::AutoBufferChar& abTarget_, data::AutoBufferChunkChar& rabcQueryValue_, size_t& rnChunkSize_);
            bool    parse_string(char* pBuffer_, size_t nBufferSize_, size_t nShift, ListPairs& lpTarget_, char cDiv, bool bTrim);
        public:
            http_text_protocol_header() :
                m_query_method_(m_abHeader)
                , m_query_uri_(m_abHeader)
                , m_query_version_(m_abHeader)
            {
                clear();
            }

            void    clear() {
                m_query_method_.clear_reference();
                m_query_uri_.clear_reference();
                m_query_version_.clear_reference();

                m_eHeaderProcess = eProcessingFirst;
                m_eBodyProcess = eNone;
                m_nPort = -1;
                m_nHost = -1;

                m_header_values.clear();
                m_params_value_.clear();
                m_cookies_.clear();

                m_abHeader.flush_free_size();
                m_abParams.flush_free_size();
                m_abBody.flush_free_size();

                m_request_.clear();
                m_response_.clear();
            }

            size_t  process_data(network::connection& rConnect_, const boost::uint8_t* pBuffer_, size_t nBufferSize_);
            bool    is_ready() const { return m_eHeaderProcess == eSuccessful && m_eBodyProcess == eSuccessful; }
            bool    is_auto_close() const { return m_response_.m_auto_close; }

            const char*    get_query_method() const { return m_query_method_.get_chunk(); }
            const char*    get_query_uri() const { return m_query_uri_.get_chunk(); }
            const char*    get_header_parameter(const std::string& param, const char* default_value = nullptr) const;
            const char*    get_parameter(const std::string& key, bool* exist_flag = nullptr) const;
            const char*    get_cookie(const std::string& sKey_, bool* pbExist_ = nullptr) const;
            const char*    get_cookie_parameter(const std::string& sKey_, const char* sDefault_ = nullptr) const;
            const boost::uint8_t*  get_body() const { return reinterpret_cast<boost::uint8_t*>(m_abBody.get_head()); }
            size_t          get_body_length() const { return m_abBody.get_fill_size(); }
            const char*     get_host();
            const char*     get_port();
            void            calculate_host_port();

            void            FillParameter2Array(const std::string& sKey_, std::vector<int>& rArray_);
            bool            get_parameter(const std::string& key, bool default_value) const;
            int             get_parameter(const std::string& key, int default_value) const;
            int64_t         get_parameter64(const std::string& key, int64_t default_value) const;
            const char*     get_parameter(const std::string& key, const char* default_value) const;
            size_t          GetParameterIndex(const std::string& sKey_) const;
            size_t          GetHeaderIndex(const std::string& sKey_) const;
            bool            is_parameter_exist(const std::string& sKey_) const;

            void    set_cookie(const std::string& cookie, const std::string& value, boost::posix_time::ptime expire, const std::string&
                               domain, bool http_only);
            void    set_response_status(const int code, const std::string& code_reason) {
                m_response_.m_nCode = code;
                m_response_.m_sCodeReason = code_reason;
            }
            void    SetRedirect(int nCode_, const std::string& sRedirectURL_) {
                m_response_.m_sRedirectURL = sRedirectURL_;
                set_response_status(nCode_, "Redirect");
            }

            void    set_content_type(const std::string& sContentType_, const std::string& sContentTypeCP_ = "UTF-8") {
                m_response_.m_sContentType = sContentType_;
                m_response_.m_sContentTypeCP = sContentTypeCP_;
            }

            void    SetContentDisposition(const std::string& sContentDisposition_) {
                m_response_.m_sContentDisposition = sContentDisposition_;
            }

            void    set_last_modify(boost::posix_time::ptime tLM_) { m_response_.m_tLastModify = tLM_; }

            bool is_send_data() const;
            void send_response(connection& connect, const char* response, size_t length);

            void send_response(connection& connect, const std::string& body) {
                send_response(connect, body.c_str(), body.size());
            }

            const char*     get_client_proxy_ip();

            bool        get_multi_part(size_t& rszIterator_, std::string& rsName_, std::string& rsContentType_, const char*& rpBuffer_, size_t& rszBuffer);

            void end_of_response(connection& connection) {
                send_response(connection, nullptr, 0);
            }

            bool    send_file(const std::string& query_uri, connection& connect, const std::string& doc_root, const std::string& file_name = "");

            response& get_response() { return m_response_; }
            const request& get_request() const { return m_request_; }
        };
        typedef	boost::shared_ptr<http_text_protocol_header>	http_text_protocol_header_ptr;

        template<typename T>
        class http_curl_t {
            static size_t WriteAutobufferCallback(void* Contents_, size_t nSize_, size_t nMemb_, void *Ctx_) {
                size_t nRealSize = (nSize_ * nMemb_) / sizeof(T);
                data::AutoBufferT<T>* pBuffer = reinterpret_cast<data::AutoBufferT<T>*>(Ctx_);

                pBuffer->check_size(pBuffer->get_fill_size() + nRealSize);
                pBuffer->append(reinterpret_cast<const T*>(Contents_), nRealSize);

                return nRealSize * sizeof(T);
            }
        protected:
            CURL*       m_curl;
        public:
            http_curl_t() {
                m_curl = curl_easy_init();
            }
            virtual ~http_curl_t() {
                if (m_curl != nullptr) {
                    /* always cleanup */
                    curl_easy_cleanup(m_curl);
                    m_curl = nullptr;
                }
            }

            bool do_request(const std::string& sURL_, data::AutoBufferT<T>& abBuffer_) {
                bool    bResult = false;

                CURLcode    curlCode, nRespCode;

                if (m_curl != nullptr) {
                    curl_easy_setopt(m_curl, CURLOPT_URL, sURL_.c_str());
                    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteAutobufferCallback);
                    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, (void *)&abBuffer_);
                    curlCode = curl_easy_perform(m_curl);
                    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &nRespCode);
                    if (nRespCode == 200) {
                        bResult = true;
                    }
                }

                return bResult;
            }
        };

        class   http_curl_string : http_curl_t<char> {
            data::AutoBufferChar    m_abBuffer;
        public:
            bool    do_easy_request(const std::string& sURL_) {
                return do_request(sURL_, m_abBuffer);
            }

            const data::AutoBufferChar&   get_buffer() const { return m_abBuffer; }
            const char* get_uri_encode(const char* pURIString_) {

                if (pURIString_ == nullptr || *pURIString_ == '\0') {
                    m_abBuffer.check_size(sizeof(char));
                    m_abBuffer[0] = '\0';
                } else {
                    int nLenghtOut = 0;

                    data::AutoBufferChar    abTemp;
                    abTemp.append(pURIString_, strlen(pURIString_));
                    // if +'s aren't replaced with %20's then curl won't unescape to spaces propperly
                    abTemp.Replace("+", 1, "%20", 3, false);
                    //            string url = std::str_replace("+", "%20", str);
                    char* pt = curl_easy_unescape(m_curl, abTemp.get_head(), static_cast<int>(abTemp.get_fill_size()), &nLenghtOut);

                    m_abBuffer.check_size(nLenghtOut);
                    strcpy(m_abBuffer.get_head(), pt);
                    m_abBuffer.flush_free_size();
                    m_abBuffer.DecrementFreeSize(nLenghtOut);
                    curl_free(pt);
                }

                return m_abBuffer.get_head();
            }
        };
    }
}
