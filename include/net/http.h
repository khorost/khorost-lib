#ifndef __HTTP_H__
#define __HTTP_H__

#include <map>
#include <string>

#ifndef WIN32
/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
#include <unistd.h>
#else
#include <winsock2.h>
#endif  // Win32

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
#define HTTP_ATTRIBUTE_REFERER               "Referer"

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


#define HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE   "keep-alive"

#define HTTP_CODEPAGE_UTF8                      "UTF-8"
#define HTTP_CODEPAGE_NULL                      ""

namespace Network {
    class httpPacket {
    public:
        static  const   char*   HTTP_QUERY_REQUEST_METHOD_GET;
        static  const   char*   HTTP_QUERY_REQUEST_METHOD_POST;
    };

    class HTTPTextProtocolHeader {
        // Быстрый доступ к атрибутам HTTP
        Data::AutoBufferChunkChar   m_abcQueryMethod;
        Data::AutoBufferChunkChar   m_abcQueryURI;
        Data::AutoBufferChunkChar   m_abcQueryVersion;

        int     m_nContentLength;
        size_t  m_nHost;
        size_t  m_nPort;

        typedef std::list< std::pair<size_t, size_t> > ListPairs;
        
        ListPairs   m_HeaderValue;  // словарь для заголовка
        ListPairs   m_ParamsValue;  // словарь для параметров
        ListPairs   m_Cookies;      // словарь для кук

        Data::AutoBufferChar                    m_abHeader;
        Data::AutoBufferChar                    m_abParams;
        Data::AutoBufferChar                    m_abBody;
        enum {
            eNone
            , eProcessingFirst
            , eProcessingNext
            , eSuccessful
            , eError
        }   m_eHeaderProcess, m_eBodyProcess;

        // ****************************************************************
        struct Replay{
            struct Cookie {
                std::string m_sCookie;
                std::string m_sValue;
                boost::posix_time::ptime      m_dtExpire;
                std::string m_sDomain;

                Cookie() {
                }
                Cookie(const Cookie& right_) {
                    if (this!=&right_) {
                        *this = right_;
                    }
                }
                Cookie(const std::string& sCookie_, const std::string& sValue_, boost::posix_time::ptime dtExpire_, const std::string& sDomain_) {
                    m_sCookie   = sCookie_;
                    m_sValue    = sValue_;
                    m_dtExpire  = dtExpire_;
                    m_sDomain   = sDomain_;
                }
                Cookie& operator=(const Cookie& right_) {
                    if (this!=&right_) {
                        m_sCookie   = right_.m_sCookie;
                        m_sValue    = right_.m_sValue;
                        m_dtExpire  = right_.m_dtExpire;
                        m_sDomain   = right_.m_sDomain;
                    }
                    return *this;
                }
            };
            std::deque<Cookie>  m_Cookies;
            bool                m_bAutoClose;
            int                 m_nCode;
            std::string         m_sCodeReason;
            std::string         m_sContentType;
            std::string         m_sContentTypeCP;
            boost::posix_time::ptime              m_tLastModify;
            std::string         m_sContentDisposition;
            std::string         m_sRedirectURL;

            void    Clear() {
                using namespace boost::posix_time;

                m_Cookies.clear();
                m_bAutoClose = true;
                m_nCode     = 200;
                m_sCodeReason   = "Ok";
                m_sContentType  = HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML;
                m_sContentTypeCP    = "UTF-8";
                m_sContentDisposition = "";
                m_tLastModify = ptime(not_a_date_time);
            }
        }   m_Replay;

        bool    GetChunk(const char*& rpBuffer_, size_t& rnBufferSize_, char cPrefix_, const char* pDiv_, Data::AutoBufferChar& abTarget_, Data::AutoBufferChunkChar& rabcQueryValue_, size_t& rnChunkSize_);
        bool    ParseString(char* pBuffer_, size_t nBufferSize_, size_t nShift, ListPairs& lpTarget_, char cDiv, bool bTrim);
    public:
        HTTPTextProtocolHeader():
          m_abcQueryMethod(m_abHeader)
        , m_abcQueryURI(m_abHeader)
        , m_abcQueryVersion(m_abHeader)
        {
            Reset();
        }

        void    Reset() {
            m_abcQueryMethod.Reset();
            m_abcQueryURI.Reset();
            m_abcQueryVersion.Reset();

            m_eHeaderProcess = eProcessingFirst;
            m_eBodyProcess   = eNone;
            m_nContentLength = -1;
            m_nPort          = -1;
            m_nHost          = -1;
            
            m_HeaderValue.clear();
            m_ParamsValue.clear();
            m_Cookies.clear();

            m_abHeader.FlushFreeSize();
            m_abParams.FlushFreeSize();
            m_abBody.FlushFreeSize();

            m_Replay.Clear();
        }

        size_t  ProcessData(Network::Connection& rConnect_, const boost::uint8_t* pBuffer_, size_t nBufferSize_);
        bool    IsReady() const {return m_eHeaderProcess==eSuccessful && m_eBodyProcess==eSuccessful;}
        bool    IsAutoClose() const {return m_Replay.m_bAutoClose;}

        const char*    GetQueryMethod() const {return m_abcQueryMethod.GetChunk();}
        const char*    GetQueryURI() const {return m_abcQueryURI.GetChunk();}
        const char*    GetHeaderParameter(const std::string& sParam_, const char* sDefault_ = NULL) const;
        const char*    GetParameter(const std::string& sKey_, bool* pbExist_ = NULL) const;
        const char*    GetCookie(const std::string& sKey_, bool* pbExist_ = NULL) const;
        const boost::uint8_t*  GetBody() const { return reinterpret_cast<boost::uint8_t*>(m_abBody.GetHead()); }
        size_t          GetBodyLength() const { return m_abBody.GetFillSize(); }
        const char*     GetHost();
        const char*     GetPort();
        void            CalculateHostPort();

        int             GetParameter(const std::string& sKey_, int nDefault_) const;
        const char*     GetParameter(const std::string& sKey_, const char* sDefault_) const;
        size_t          GetParameterIndex(const std::string& sKey_) const;
        size_t          GetHeaderIndex(const std::string& sKey_) const;
        bool            IsParameterExist(const std::string& sKey_) const;

        void    SetCookie(const std::string& sCookie_, const std::string& sValue_, boost::posix_time::ptime dtExpire_, const std::string& sDomain_);
        void    SetResponseStatus(int nCode_, const std::string& sCodeReason_) {
            m_Replay.m_nCode = nCode_;
            m_Replay.m_sCodeReason = sCodeReason_;
        }
        void    SetRedirect(int nCode_, const std::string& sRedirectURL_) {
            m_Replay.m_sRedirectURL = sRedirectURL_;
            SetResponseStatus(nCode_, "Redirect");
        }

        void    SetContentType(const std::string& sContentType_, const std::string& sContentTypeCP_ = "UTF-8") {
            m_Replay.m_sContentType     = sContentType_;
            m_Replay.m_sContentTypeCP   = sContentTypeCP_;
        }

        void    SetContentDisposition(const std::string& sContentDisposition_) {
            m_Replay.m_sContentDisposition = sContentDisposition_;
        }

        void    SetLastModify(boost::posix_time::ptime tLM_) { m_Replay.m_tLastModify = tLM_; }

        void Response(Network::Connection& rConnect_, const char* psResponse_, size_t nLength = -1);

        const char*     GetClientProxyIP();

        bool        GetMultiPart(size_t& rszIterator_, std::string& rsName_, std::string& rsContentType_, const char*& rpBuffer_, size_t& rszBuffer);
    };

    class HTTPFileTransfer {
    public:
        HTTPFileTransfer() {}
        virtual ~HTTPFileTransfer() {}

        bool    SendFile(Network::Connection& rConnect_, HTTPTextProtocolHeader& rHTTP_, const std::string& sDocRoot_, const std::string& sFileName_="");
    };

    template<typename T>
    class HTTPCurlT {
        static size_t WriteAutobufferCallback(void* Contents_, size_t nSize_, size_t nMemb_, void *Ctx_) {
            size_t nRealSize = (nSize_ * nMemb_) / sizeof(T);
            Data::AutoBufferT<T>* pBuffer = reinterpret_cast<Data::AutoBufferT<T>* >(Ctx_);

            pBuffer->CheckSize(pBuffer->GetFillSize() + nRealSize);
            pBuffer->Append(reinterpret_cast<const T*>(Contents_), nRealSize);

            return nRealSize * sizeof(T);
        }
    protected:
        CURL*       m_curl;
    public:
        HTTPCurlT() {
            m_curl = curl_easy_init();
        }
        virtual ~HTTPCurlT() {
            if (m_curl!=NULL) {
                /* always cleanup */
                curl_easy_cleanup(m_curl);
                m_curl = NULL;
            }
        }

        bool DoRequest(const std::string& sURL_, Data::AutoBufferT<T>& abBuffer_) {
            bool    bResult = false;

            CURLcode    curlCode, nRespCode;

            if (m_curl!=NULL) {
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

    class   HTTPCurlString : HTTPCurlT<char> {
        Data::AutoBufferChar    m_abBuffer;
    public:
        bool    DoEasyRequest(const std::string& sURL_) {
            return DoRequest(sURL_, m_abBuffer);
        }

        const Data::AutoBufferChar&   GetBuffer() const {return m_abBuffer;}
        const char* GetURIEncode(const char* pURIString_) {

            if (pURIString_ == NULL || *pURIString_ == '\0') {
                m_abBuffer.CheckSize(sizeof(char));
                m_abBuffer[0] = '\0';
            } else {
                int nLenghtOut = 0;

                Data::AutoBufferChar    abTemp;
                abTemp.Append(pURIString_, strlen(pURIString_));
                // if +'s aren't replaced with %20's then curl won't unescape to spaces propperly
                abTemp.Replace("+", 1, "%20", 3, false);
                //            string url = std::str_replace("+", "%20", str);
                char* pt = curl_easy_unescape(m_curl, abTemp.GetHead(), abTemp.GetFillSize(), &nLenghtOut);

                m_abBuffer.CheckSize(nLenghtOut);
                strcpy(m_abBuffer.GetHead(), pt);
                m_abBuffer.FlushFreeSize();
                m_abBuffer.DecrementFreeSize(nLenghtOut);
                curl_free(pt);
            }

            return m_abBuffer.GetHead();
        }
    };
}

#endif //   __HTTP_H__
