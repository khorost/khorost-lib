
#ifndef NOMINMAX
 #define NOMINMAX
#endif

#include <string>
#include <algorithm>
#include <time.h>

#include "net/http.h"
#include "system/fastfile.h"
#include "util/utils.h"

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

const char*   http_packet::HTTP_QUERY_REQUEST_METHOD_GET     = "GET";
const char*   http_packet::HTTP_QUERY_REQUEST_METHOD_POST    = "POST";

bool FindSubValue(const char* pSource_, size_t nSourceSize_, const char* pMatch_, size_t nMatchSize_, char cDivKV, char cDivKK, const char** pResult_ = nullptr, size_t* pnResultSize_ = nullptr) {
    if (pSource_ == nullptr) {
        return false;
    }

    if (nSourceSize_ == -1) {
        nSourceSize_ = strlen(pSource_);
    }

    if (nSourceSize_ < nMatchSize_) {
        return false;
    }

    auto    pMax = nSourceSize_ - nMatchSize_;
    bool    bInQuote = false;
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
                        } else if (pSource_[pCheck] == cDivKK){
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

bool http_text_protocol_header::GetChunk(const char*& rpBuffer_, size_t& rnBufferSize_, char cPrefix_, const char* pDiv_, data::AutoBufferChar& abTarget_, data::AutoBufferChunkChar& rabcQueryValue_, size_t& rnChunkSize_) {
    size_t s;
    // зачишаем префикс от white символов
    for (s=0; s<rnBufferSize_ && rpBuffer_[s]==cPrefix_; ++s) {
    }

    for (size_t k=s; k<rnBufferSize_; ++k) {
        // выделяем chunk до stop символов
        for (size_t t=0; pDiv_[t]!='\0'; ++t) {
            if (rpBuffer_[k]==pDiv_[t]) {
                // stop символ найден, chunk полный
                t = abTarget_.GetFillSize();
                rnChunkSize_ = k + sizeof(char);
                abTarget_.Append(rpBuffer_, rnChunkSize_);
                abTarget_[t + k] = '\0'; // заменяем stop символ на завершение zстроки
                rabcQueryValue_.SetReference(t + s);

                rpBuffer_       += rnChunkSize_;
                rnBufferSize_   -= rnChunkSize_;

                return true;
            }
        }
    }
    return false;
}

size_t  http_text_protocol_header::process_data(network::connection& rConnect_, const boost::uint8_t *pBuffer_, size_t nBufferSize_) {
    size_t nProcessByte = 0, nChunkSize, nChunkSizeV;
    switch (m_eHeaderProcess) {
    case eProcessingFirst:
        //  GET / POST ****************************************************
        if (!m_abcQueryMethod.IsValid()) {
            if (!GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, ' ', " ", m_abHeader, m_abcQueryMethod, nChunkSize)) {
                return nProcessByte;
            } else {
                nProcessByte    += nChunkSize;
            }
        }
        //  /index.html ***************************************************
        if (!m_abcQueryURI.IsValid()) {
            if (!GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, ' ', "? ", m_abHeader, m_abcQueryURI, nChunkSize)) {
                return nProcessByte;
            } else {
                if (*(pBuffer_ - sizeof(char))=='?') {
                    --nChunkSize;
                    pBuffer_        -= sizeof(char);
                    nBufferSize_    += sizeof(char);
                }
                nProcessByte    += nChunkSize;
            }
        }
        //  ?key=val& ***************************************************
        if (m_abParams.GetFillSize()==0 && pBuffer_[0]=='?') {
            data::AutoBufferChunkChar   abcParam(m_abParams);
            pBuffer_ += sizeof(char);
            nBufferSize_ -= sizeof(char);
            if (!GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, '?', " ", m_abParams, abcParam, nChunkSize)) {
                return nProcessByte;
            } else {
                nProcessByte    += nChunkSize + sizeof(char);
            }
        }
        //  HTTP/1.1   ****************************************************
        if (!m_abcQueryVersion.IsValid()) {
            if (!GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, ' ', "\r\n", m_abHeader, m_abcQueryVersion, nChunkSize)) {
                return nProcessByte;
            } else {
                if (0<nBufferSize_ && (pBuffer_[0]=='\r' || pBuffer_[0]=='\n')) {
                    ++nChunkSize;
                    pBuffer_        += sizeof(char);
                    nBufferSize_    -= sizeof(char);
                }
                nProcessByte    += nChunkSize;

                m_eHeaderProcess = eProcessingNext;
            }
        }
    case eProcessingNext:
        while (nBufferSize_>=2*sizeof(char)) {
            if (pBuffer_[0]=='\r' && pBuffer_[1]=='\n') {
                m_eHeaderProcess = eSuccessful;
                m_eBodyProcess = eProcessingFirst;

                pBuffer_        += 2*sizeof(char);
                nBufferSize_    -= 2*sizeof(char);
                nProcessByte    += 2*sizeof(char);
                break;
            }
            
            data::AutoBufferChunkChar   abcHeaderKey(m_abHeader);
            data::AutoBufferChunkChar   abcHeaderValue(m_abHeader);

            if (GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, ' ', ":", m_abHeader, abcHeaderKey, nChunkSize) 
                && GetChunk(reinterpret_cast<const char*&>(pBuffer_), nBufferSize_, ' ', "\r\n", m_abHeader, abcHeaderValue, nChunkSizeV)) {
                if (0<nBufferSize_ && (pBuffer_[0]=='\r' || pBuffer_[0]=='\n')) {
                    ++nChunkSize;
                    pBuffer_        += sizeof(char);
                    nBufferSize_    -= sizeof(char);
                }
                nProcessByte    += nChunkSize + nChunkSizeV;

                m_HeaderValue.push_back(std::make_pair(abcHeaderKey.GetReference(), abcHeaderValue.GetReference()));
                if (m_nContentLength==-1 && strcmp(HTTP_ATTRIBUTE_CONTENT_LENGTH, abcHeaderKey.GetChunk())==0) {
                    m_nContentLength  = atoi(abcHeaderValue.GetChunk());
                } else if (strcmp(HTTP_ATTRIBUTE_COOKIE, abcHeaderKey.GetChunk())==0) {
                    ParseString(const_cast<char*>(abcHeaderValue.GetChunk()), nChunkSizeV, abcHeaderValue.GetReference(), m_Cookies, ';', true);
                }
            }  else {
                m_abHeader.DecrementFreeSize(nChunkSize);
                break;
            }
        }
        break;
    }

    if (m_eBodyProcess==eProcessingFirst || m_eBodyProcess==eProcessingNext) {
        nChunkSize = std::min(nBufferSize_, (size_t)m_nContentLength);
        if (m_nContentLength==-1 || (nBufferSize_==0 && m_nContentLength==0)) {
            m_eBodyProcess = eSuccessful;
        } else if(nChunkSize!=0) {
            m_abBody.Append(reinterpret_cast<const char*>(pBuffer_), nChunkSize);
            nProcessByte += nChunkSize;

            if (m_abBody.GetFillSize() >= (size_t)m_nContentLength) {
                m_eBodyProcess = eSuccessful;

                const char *pszContentType = GetHeaderParameter(HTTP_ATTRIBUTE_CONTENT_TYPE);
                if (pszContentType!= nullptr && FindSubValue(pszContentType, -1, HTTP_ATTRIBUTE_CONTENT_TYPE__FORM, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__FORM) - 1, '=', ';')) {
//                if (strcmp(HTTP_ATTRIBUTE_CONTENT_TYPE__FORM, pszContentType) == 0) {
                    size_t k = m_abParams.GetFillSize();
                    if (k != 0) {
                        if (m_abParams.GetElement(k - 1) == '\0') {
                            if (k > 1 && m_abParams.GetElement(k - 2) != '&') {
                                m_abParams[k - 1] = '&';
                            }
                            else {
                                m_abParams.IncrementFreeSize(sizeof(char));
                            }
                        }
                        else if (m_abParams.GetElement(k - 1) != '&') {
                            m_abParams.Append("&", sizeof(char));
                        }
                        m_nContentLength += static_cast<int>(m_abParams.GetFillSize());
                    }

                    m_abParams.Append(m_abBody.GetHead(), m_abBody.GetFillSize());
                    m_abParams.Append("\0", sizeof(char));
                }
            }
        }
        if (m_eBodyProcess==eSuccessful) {
            ParseString(m_abParams.GetPosition(0), m_abParams.GetFillSize(), 0, m_ParamsValue, '&', false);
        }
    }

    return nProcessByte;
}

bool http_text_protocol_header::GetMultiPart(size_t& rszIterator_, std::string& rsName_, std::string& rsContentType_, const char*& rpBuffer_, size_t& rszBuffer) {
    const char *pszContentType = GetHeaderParameter(HTTP_ATTRIBUTE_CONTENT_TYPE);
    if (pszContentType == nullptr) {
        return false;
    }
    
    const char *pszBoundary = nullptr;
    size_t szCT = strlen(pszContentType), szBoundary = 0;

    if (FindSubValue(pszContentType, szCT, HTTP_ATTRIBUTE_CONTENT_TYPE__MULTIPART_FORM_DATA, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__MULTIPART_FORM_DATA) - 1, '=', ';')) {
        if (FindSubValue(pszContentType, szCT, HTTP_ATTRIBUTE_CONTENT_TYPE__BOUNDARY, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE__BOUNDARY) - 1, '=', ';', &pszBoundary, &szBoundary)) {
            std::string sBoundary = HTTP_ATTRIBUTE_LABEL_CD;
            sBoundary.append(pszBoundary, szBoundary);
            // проверить что контрольная метка правильная
            if (m_abBody.Compare(rszIterator_, sBoundary.c_str(), sBoundary.size()) != 0) {
                return false;
            }
            rszIterator_ += sBoundary.size();

            if (m_abBody.Compare(rszIterator_, HTTP_ATTRIBUTE_LABEL_CD, sizeof(HTTP_ATTRIBUTE_LABEL_CD) - 1) == 0) {
                return false; // завершение блока
            } else if (m_abBody.Compare(rszIterator_, HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1) != 0) {
                return false;
            }
            rszIterator_ += sizeof(HTTP_ATTRIBUTE_ENDL) - 1;
            auto    pMax = m_abBody.GetFillSize();
            while (rszIterator_ < pMax) {
                auto pEndLine = m_abBody.Find(rszIterator_, HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
                if (pEndLine == std::string::npos) {
                    pEndLine = m_abBody.GetFillSize() - rszIterator_;
                }
                if (pEndLine == rszIterator_) {
                    rszIterator_ += sizeof(HTTP_ATTRIBUTE_ENDL) - 1;
                    sBoundary.insert(0, HTTP_ATTRIBUTE_ENDL);
                    pEndLine = m_abBody.Find(rszIterator_, sBoundary.c_str(), sBoundary.size());
                    if (pEndLine == std::string::npos) {
                        return false;
                    }

                    rpBuffer_ = m_abBody.GetHead() + rszIterator_;
                    rszBuffer = pEndLine - rszIterator_;
                    rszIterator_ = pEndLine;
                    return true;
                } else {
                    const char* pValue;
                    size_t szValue;
                    if (m_abBody.Compare(rszIterator_, HTTP_ATTRIBUTE_CONTENT_DISPOSITION, sizeof(HTTP_ATTRIBUTE_CONTENT_DISPOSITION) - 1) == 0) {
                        if (FindSubValue(m_abBody.GetHead() + rszIterator_, pEndLine - rszIterator_, "name", sizeof("name") - 1, '=', ';', &pValue, &szValue)) {
                            rsName_.assign(pValue, szValue);
                        }
                    } else if (m_abBody.Compare(rszIterator_, HTTP_ATTRIBUTE_CONTENT_TYPE, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE) - 1) == 0) {
                        if (FindSubValue(m_abBody.GetHead() + rszIterator_, pEndLine - rszIterator_, HTTP_ATTRIBUTE_CONTENT_TYPE, sizeof(HTTP_ATTRIBUTE_CONTENT_TYPE) - 1, ':', ';', &pValue, &szValue)) {
                            rsContentType_.assign(pValue, szValue);
                        }
                    }
                    rszIterator_ = pEndLine + sizeof(HTTP_ATTRIBUTE_ENDL) - 1;
                }
            }
        }
    }
    
// Content-Type: multipart/form-data; boundary=xmyidlatfdmcrqnk; charset=UTF-8
// Content-Disposition: form-data; name="image"; filename="1.png"
//    Content - Type: image / png


    return false;
}

bool http_text_protocol_header::ParseString(char* pBuffer_, size_t nBufferSize_, size_t nShift, ListPairs& lpTarget_, char cDiv, bool bTrim) {
    size_t k, v, t;
    for (k=0, v=-1, t=0;t<nBufferSize_;) {
        if (t>=nBufferSize_ || pBuffer_[t]=='\0') {
            if (bTrim){
                for(;pBuffer_[k]!='\0' && pBuffer_[k]==' ';++k){
                }
            }
            lpTarget_.push_back(std::make_pair(nShift + k, v!=-1?nShift + v:v));
            break;
        } else if (pBuffer_[t]==cDiv) {
            pBuffer_[t] = '\0';
            if (bTrim){
                for(;pBuffer_[k]!='\0' && pBuffer_[k]==' ';++k){
                }
            }
            lpTarget_.push_back(std::make_pair(nShift + k, v!=-1?nShift + v:v));
            k = ++t;
            v = -1;
        } else if (pBuffer_[t]=='=') {
            pBuffer_[t] = '\0';
            v = ++t;
        } else {
            ++t;
        }
    }
    return true;
}

const char* http_text_protocol_header::GetCookieParameter(const std::string& sKey_, const char* sDefault_) const {
    const char* psValue = get_cookie(sKey_);

    if (psValue == nullptr) {
        psValue = GetParameter(sKey_, sDefault_);
    }
    return psValue;
}

const char* http_text_protocol_header::get_cookie(const std::string& sKey_, bool* pbExist_) const {
    if (pbExist_!= nullptr) {
        *pbExist_ = false;
    }

    for (const auto& cit : m_Cookies) {
        if (strcmp(sKey_.c_str(), m_abHeader.GetPosition(cit.first)) == 0) {
            if (pbExist_ != nullptr) {
                *pbExist_ = true;
            }
            return cit.second != -1 ? m_abHeader.GetPosition(cit.second) : nullptr;
        }
    }

    return nullptr;
}

bool http_text_protocol_header::IsParameterExist(const std::string& sKey_) const {
    for (const auto& cit : m_ParamsValue) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit.first)) == 0) {
            return true;
        }
    }
    return false;
}

size_t http_text_protocol_header::GetParameterIndex(const std::string& sKey_) const {
    for (const auto& cit : m_ParamsValue) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit.first)) == 0) {
            return cit.second;
        }
    }
    return -1;
}

size_t http_text_protocol_header::GetHeaderIndex(const std::string& sKey_) const {
    for (const auto& cit : m_HeaderValue) {
        if (strcmp(sKey_.c_str(), m_abHeader.GetPosition(cit.first)) == 0) {
            return cit.second;
        }
    }
    return -1;
}

void http_text_protocol_header::FillParameter2Array(const std::string& sKey_, std::vector<int>& rArray_) {
    std::string sKey2 = sKey_ + "[]";
    for (const auto& p : m_ParamsValue) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(p.first)) == 0 || strcmp(sKey2.c_str(), m_abParams.GetPosition(p.first)) == 0) {
            rArray_.push_back(atoi( m_abParams.GetPosition(p.second)));
        }
    }
}

const char* http_text_protocol_header::GetParameter(const std::string& sKey_, bool* pbExist_) const {
    if (pbExist_!= nullptr) {
        *pbExist_ = false;
    }

    for (const auto& cit : m_ParamsValue) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit.first)) == 0) {
            if (pbExist_ != nullptr) {
                *pbExist_ = true;
            }
            return cit.second != -1 ? m_abParams.GetPosition(cit.second) : nullptr;
        }
    }

    return nullptr;
}

const char* http_text_protocol_header::GetHeaderParameter(const std::string& sParam_, const char* sDefault_) const {
    for (const auto& cit : m_HeaderValue) {
        if (strcmp(sParam_.c_str(), m_abHeader.GetPosition(cit.first)) == 0) {
            return m_abHeader.GetPosition(cit.second);
        }
    }

    return sDefault_;
}

void http_text_protocol_header::response(network::connection& connect, const char* response, const size_t length) {
    using namespace boost::posix_time;

    char  st[255];

    connect.SendString(m_abcQueryVersion.GetChunk());
    connect.SendString(" ", sizeof(char));
    connect.SendNumber(m_Replay.m_nCode);
    connect.SendString(" ", sizeof(char));
    connect.SendString(m_Replay.m_sCodeReason);
    connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    connect.SendString("Server: phreeber" HTTP_ATTRIBUTE_ENDL);
    time_t n = time(nullptr);
    strftime(st, sizeof(st), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&n));
    connect.SendString("Date: ");
    connect.SendString(st);
    connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);

    if (!m_Replay.m_sRedirectURL.empty()) {
        connect.SendString("Location: ");
        connect.SendString(m_Replay.m_sRedirectURL);
        connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);
    }

    const auto pAC = GetHeaderParameter(HTTP_ATTRIBUTE_CONNECTION);
    if (pAC== nullptr || strcmp(HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE, pAC)!=0) {
        m_Replay.m_bAutoClose = true;
    } else {
        m_Replay.m_bAutoClose = false;
    }
    connect.SendString(
        std::string("Connection: ") + std::string(pAC != nullptr ? pAC : "close") + std::string(HTTP_ATTRIBUTE_ENDL));

    // CORS
    const auto origin = GetHeaderParameter(HTTP_ATTRIBUTE__ORIGIN);
    if (origin != nullptr) {
        connect.SendString(HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_ORIGIN ": " + std::string(origin) + std::string(HTTP_ATTRIBUTE_ENDL));
        connect.SendString(HTTP_ATTRIBUTE__ACCESS_CONTROL_ALLOW_CREDENTIALS ": true" HTTP_ATTRIBUTE_ENDL);
    }

    for (const auto& c : m_Replay.m_Cookies) {
        time_t gmt = data::EpochDiff(c.m_dtExpire).total_seconds();
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&gmt));

        connect.SendString(
            std::string("Set-Cookie: ") + c.m_sCookie + std::string("=") + c.m_sValue + std::string("; Expires=") +
            std::string(st) + std::string("; Domain=.") + c.m_sDomain + std::string("; Path=/") +
            (c.m_http_only ? std::string("; HttpOnly ") : "") + HTTP_ATTRIBUTE_ENDL);
    }

    connect.SendString(std::string(HTTP_ATTRIBUTE_CONTENT_TYPE HTTP_ATTRIBUTE_DIV) + m_Replay.m_sContentType);
    if (!m_Replay.m_sContentTypeCP.empty()) {
        connect.SendString(std::string("; charset=") + m_Replay.m_sContentTypeCP);
    }
    connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);

    if (!m_Replay.m_sContentDisposition.empty()) {
        connect.SendString(std::string(HTTP_ATTRIBUTE_CONTENT_DISPOSITION HTTP_ATTRIBUTE_DIV) + m_Replay.m_sContentDisposition);
        connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    }

    connect.SendString(HTTP_ATTRIBUTE_CONTENT_LENGTH ": ");
    connect.SendNumber(static_cast<unsigned int>(length));
    connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL) - 1);

    if (!m_Replay.m_tLastModify.is_not_a_date_time()) {
        connect.SendString(HTTP_ATTRIBUTE_LAST_MODIFIED HTTP_ATTRIBUTE_DIV);
        time_t gmt = data::EpochDiff(m_Replay.m_tLastModify).total_seconds();
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&gmt));
        connect.SendString(st);
        connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    }
    connect.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    if (response!= nullptr) {
        connect.SendString(response, length);
    }
}

void http_text_protocol_header::set_cookie(const std::string& cookie, const std::string& value, boost::posix_time::ptime expire, const std::string& domain, bool http_only) {
    m_Replay.m_Cookies.emplace_back(cookie, value, expire, domain, http_only);
}

#ifdef WIN32
#define DIRECTORY_DIV   '\\'
static bool IsSlash (char ch_) { return ch_=='/'; }
#else
#define DIRECTORY_DIV   '/'
#endif


bool http_text_protocol_header::send_file(const std::string& query_uri, network::connection& connect, const std::string& doc_root, const std::string& file_name) {
    using namespace boost::posix_time;

    // TODO сделать корректировку по абсолютно-относительным переходам
    std::string     sFileName;
    if(file_name.empty()) {
        sFileName = doc_root + query_uri;
    } else {
        sFileName = doc_root + file_name;
    }

#ifdef WIN32
    std::replace_if(sFileName.begin(), sFileName.end(), IsSlash, DIRECTORY_DIV);

    DWORD dwAttr = GetFileAttributes(sFileName.c_str());
#else
    struct stat s;
#endif

    if (*sFileName.rbegin()==DIRECTORY_DIV) {
        sFileName += "index.html";
#ifdef WIN32
    } else if (dwAttr!=-1 && dwAttr&FILE_ATTRIBUTE_DIRECTORY) {
#else
    } else if (stat(sFileName.c_str(), &s)!=-1 && S_ISDIR(s.st_mode)) {
#endif
        sFileName.append(1, DIRECTORY_DIV);
        sFileName += "index.html";
    }

    std::string sCanonicFileName;

    if (sFileName.length() >= MAX_PATH) {
        LOGF(WARNING, "Path is too long");
        
        set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        response(connect, "File not found");
        return false;
    }

    char        bufCanonicFileName[MAX_PATH];
    memset(bufCanonicFileName, 0, sizeof(bufCanonicFileName));

#ifdef WIN32
    if (!PathCanonicalize((LPSTR)bufCanonicFileName, sFileName.c_str())) {
        LOGF(WARNING, "PathCanonicalize failed");

        set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        response(connect, "File not found");
        return false;
    }
#else
    if (!realpath(sFileName.c_str(), bufCanonicFileName)) {
        LOGF(WARNING, "realpath failed");

        http.set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        http.response(connect, "File not found");
        return false;
    }
#endif

    sCanonicFileName = bufCanonicFileName;

    if (sCanonicFileName.substr(0, doc_root.length()) != doc_root) {
        LOGF(WARNING, "Access outside of docroot attempted");

        set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        response(connect, "File not found");
        return false;
    }

    System::FastFile    ff;
    if (ff.Open(sCanonicFileName, -1, true)) {
        static struct SECT {
            const char* m_Ext;
            const char* m_CT;
            const char* m_CP;
        }   s_SECT[] = {
            { "js", HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS, HTTP_CODEPAGE_UTF8 }
            , { "html", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8 }
            , { "htm", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8 }
            , { "css", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_CSS, HTTP_CODEPAGE_UTF8 }
            , { "gif", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_GIF, HTTP_CODEPAGE_NULL }
            , { "jpg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL }
            , { "jpeg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL }
            , { "png", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_PNG, HTTP_CODEPAGE_NULL }
            , { nullptr, nullptr, nullptr }
        };

        const char* pIMS = GetHeaderParameter("If-Modified-Since");
        if (pIMS != nullptr) {
            tm t;
            strptime(pIMS, "%a, %d-%b-%Y %H:%M:%S GMT", &t);
#ifdef WIN32
            time_t tt = _mkgmtime(&t);
#else
            time_t tt = timegm(&t);
#endif  // WIN32
            if (tt >= ff.GetTimeUpdate()) {
                LOGF(DEBUG, "Dont send file '%s' length = %zu. Response 304 (If-Modified-Since: '%s')", query_uri.c_str(), ff.GetLength(), pIMS);

                set_response_status(304, "Not Modified");
                response(connect, nullptr, 0);
                return true;
            }
        }

        const char*   pExt = sCanonicFileName.c_str();
        size_t  nExt = sCanonicFileName.size();
        for (; nExt > 0; --nExt) {
            if (pExt[nExt] == '.') {
                nExt++;
                break;
            } else if (pExt[nExt] == '\\' || pExt[nExt] == '/') {
                nExt = 0;
                break;
            }
        }

        LOGF(DEBUG, "Send file '%s' length = %zu", query_uri.c_str(), ff.GetLength());

        if (nExt > 0) {
            pExt += nExt;
            for (int k = 0; s_SECT[k].m_Ext != nullptr; ++k) {
                if (strcmp(s_SECT[k].m_Ext, pExt) == 0) {
                    set_content_type(s_SECT[k].m_CT, s_SECT[k].m_CP);
                    break;
                }
            }
        }

        SetLastModify(from_time_t(ff.GetTimeUpdate()));
        response(connect, nullptr, ff.GetLength());

        connect.SendData(reinterpret_cast<const boost::uint8_t*>(ff.GetMemory()), ff.GetLength());

        ff.Close();
    } else {
        LOGF(WARNING, "File not found '%s'", sCanonicFileName.c_str());

        set_response_status(HTTP_RESPONSE_STATUS_NOT_FOUND, "Not found");
        response(connect, "File not found");
        return false;
    }

    return true;
}

bool http_text_protocol_header::GetParameter(const std::string& sKey_, bool bDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist ? strcmp(pValue, "true") == 0 : bDefault_;
}

int http_text_protocol_header::GetParameter(const std::string& sKey_, int nDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist?atoi(pValue):nDefault_;
}

int64_t http_text_protocol_header::GetParameter64(const std::string& sKey_, int64_t nDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist ? atoll(pValue) : nDefault_;
}

const char* http_text_protocol_header::GetParameter(const std::string& sKey_, const char* sDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist?pValue:sDefault_;
}

void http_text_protocol_header::CalculateHostPort() {
    size_t idx = GetHeaderIndex(HTTP_ATTRIBUTE_HOST);
    if (idx==-1) {
        return;
    }
    m_nHost = idx;

    for (;;++idx) {
        char ch = *m_abHeader.GetPosition(idx);
        if (ch=='\0') {
            break;
            m_nPort = 0;
        } else if (ch==':') {
            m_abHeader[idx] = '\0';
            m_nPort = idx + 1;
            break;
        }
    }
}

const char* http_text_protocol_header::GetHost() {
    if (m_nHost==-1) {
        CalculateHostPort();
    }
    return m_nHost==-1?"":m_abHeader.GetPosition(m_nHost);
}

const char* http_text_protocol_header::GetPort() {
    if (m_nHost==-1) {
        CalculateHostPort();
    }
    return (m_nPort==-1 || m_nPort==0)?"":m_abHeader.GetPosition(m_nPort);
}

const char* http_text_protocol_header::GetClientProxyIP() {
    size_t  idx = GetHeaderIndex(HTTP_ATTRIBUTE_X_FORWARDED_FOR);
    if (idx==-1) {
        idx = GetHeaderIndex(HTTP_ATTRIBUTE_X_REAL_IP);
        if (idx==-1) {
            return nullptr;
        }
    }
    return m_abHeader.GetPosition(idx);
}
