#define NOMINMAX

#include <string>
#include <algorithm>
#include <time.h>

#include "net/http.h"

#include "system/fastfile.h"

#ifdef WIN32
#include "shlwapi.h"
#include "win/strptime.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#endif

#pragma comment(lib,"shlwapi.lib")

using namespace Network;

const char*   httpPacket::HTTP_QUERY_REQUEST_METHOD_GET     = "GET";
const char*   httpPacket::HTTP_QUERY_REQUEST_METHOD_POST    = "POST";

bool HTTPTextProtocolHeader::GetChunk(const char*& rpBuffer_, size_t& rnBufferSize_, char cPrefix_, const char* pDiv_, Data::AutoBufferChar& abTarget_, Data::AutoBufferChunkChar& rabcQueryValue_, size_t& rnChunkSize_) {
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

bool HTTPTextProtocolHeader::GetBody(std::string& rContent_) {
    rContent_.assign(reinterpret_cast<const char*>(m_abBody.GetHead()), m_abBody.GetFillSize());
    return true;
}

size_t  HTTPTextProtocolHeader::ProcessData(Network::Connection& rConnect_, const boost::uint8_t *pBuffer_, size_t nBufferSize_) {
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
            Data::AutoBufferChunkChar   abcParam(m_abParams);
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
            
            Data::AutoBufferChunkChar   abcHeaderKey(m_abHeader);
            Data::AutoBufferChunkChar   abcHeaderValue(m_abHeader);

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
        nChunkSize = std::min(nBufferSize_, static_cast<size_t>(m_nContentLength));
        if (m_nContentLength==-1 || (nBufferSize_==0 && m_nContentLength==0)) {
            m_eBodyProcess = eSuccessful;
        } else if(nChunkSize!=0) {
            if (strcmp(HTTP_ATTRIBUTE_CONTENT_TYPE__FORM, GetHeaderParameter(HTTP_ATTRIBUTE_CONTENT_TYPE))==0) {
                size_t k = m_abParams.GetFillSize();
                if (m_eBodyProcess==eProcessingFirst && k!=0) {
                    if (m_abParams.GetElement(k-1)=='\0') {
                        if (k>1 && m_abParams.GetElement(k-2)!='&'){
                            m_abParams[k-1] = '&';
                        } else {
                            m_abParams.IncrementFreeSize(sizeof(char));
                        }
                    } else if (m_abParams.GetElement(k-1)!='&') {
                        m_abParams.Append("&", sizeof(char));
                    }
                    m_nContentLength += m_abParams.GetFillSize();
                }
                m_eBodyProcess = eProcessingNext;
                m_abParams.Append(reinterpret_cast<const char*>(pBuffer_), nChunkSize);
                m_abParams.Append("\0", sizeof(char));
                nProcessByte += nChunkSize;
                if (m_abParams.GetFillSize()>=(size_t)m_nContentLength){
                    m_eBodyProcess = eSuccessful;
                }
            } else {
                m_abBody.Append(pBuffer_, nChunkSize);
                nProcessByte += nChunkSize;
                if (m_abBody.GetFillSize()>=(size_t)m_nContentLength){
                    m_eBodyProcess = eSuccessful;
                }
            }
        }
        if (m_eBodyProcess==eSuccessful) {
            ParseString(m_abParams.GetPosition(0), m_abParams.GetFillSize(), 0, m_ParamsValue, '&', false);
        }
    }

    return nProcessByte;
}

bool HTTPTextProtocolHeader::ParseString(char* pBuffer_, size_t nBufferSize_, size_t nShift, ListPairs& lpTarget_, char cDiv, bool bTrim) {
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

const char* HTTPTextProtocolHeader::GetCookie(const std::string& sKey_, bool* pbExist_) const {
    if (pbExist_!=NULL) {
        *pbExist_ = false;
    }

    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_Cookies.begin(); cit!=m_Cookies.end(); ++cit) {
        if (strcmp(sKey_.c_str(), m_abHeader.GetPosition(cit->first))==0) {
            if (pbExist_!=NULL) {
                *pbExist_ = true;
            }
            return cit->second!=-1?m_abHeader.GetPosition(cit->second):NULL;
        }
    }

    return NULL;
}

bool HTTPTextProtocolHeader::IsParameterExist(const std::string& sKey_) const {
    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_ParamsValue.begin(); cit!=m_ParamsValue.end(); ++cit) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit->first))==0) {
            return true;
        }
    }
    return false;
}

size_t HTTPTextProtocolHeader::GetParameterIndex(const std::string& sKey_) const {
    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_ParamsValue.begin(); cit!=m_ParamsValue.end(); ++cit) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit->first))==0) {
            return cit->second;
        }
    }
    return -1;
}

size_t HTTPTextProtocolHeader::GetHeaderIndex(const std::string& sKey_) const {
    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_HeaderValue.begin(); cit!=m_HeaderValue.end(); ++cit) {
        if (strcmp(sKey_.c_str(), m_abHeader.GetPosition(cit->first))==0) {
            return cit->second;
        }
    }
    return -1;
}

const char* HTTPTextProtocolHeader::GetParameter(const std::string& sKey_, bool* pbExist_) const {
    if (pbExist_!=NULL) {
        *pbExist_ = false;
    }

    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_ParamsValue.begin(); cit!=m_ParamsValue.end(); ++cit) {
        if (strcmp(sKey_.c_str(), m_abParams.GetPosition(cit->first))==0) {
            if (pbExist_!=NULL) {
                *pbExist_ = true;
            }
            return cit->second!=-1?m_abParams.GetPosition(cit->second):NULL;
        }
    }

    return NULL;
}

const char* HTTPTextProtocolHeader::GetHeaderParameter(const std::string& sParam_, const char* sDefault_) const {
    for (std::list< std::pair<size_t, size_t> >::const_iterator cit=m_HeaderValue.begin(); cit!=m_HeaderValue.end(); ++cit) {
        if (strcmp(sParam_.c_str(), m_abHeader.GetPosition(cit->first))==0) {
            return m_abHeader.GetPosition(cit->second);
        }
    }

    return sDefault_;
}

void HTTPTextProtocolHeader::Response(Network::Connection& rConnect_, const char* psResponse_, size_t nLength) {
    char  st[255];

    if (nLength==-1 && psResponse_!=NULL) {
//        ASSERT(psResponse_!=NULL);
        nLength = strlen(psResponse_);
    }

    rConnect_.SendString(m_abcQueryVersion.GetChunk());
    rConnect_.SendString(" ", sizeof(char));
    rConnect_.SendNumber(m_Replay.m_nCode);
    rConnect_.SendString(" ", sizeof(char));
    rConnect_.SendString(m_Replay.m_sCodeReason);
    rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    rConnect_.SendString("Server: phreeber" HTTP_ATTRIBUTE_ENDL);
    const char* pAC = GetHeaderParameter(HTTP_ATTRIBUTE_CONNECTION);
    if (pAC==NULL || strcmp(HTTP_ATTRIBUTE_CONNECTION__KEEP_ALIVE, pAC)!=0) {
        m_Replay.m_bAutoClose = true;
    } else {
        m_Replay.m_bAutoClose = false;
    }
    rConnect_.SendString(std::string("Connection: ") + std::string(pAC!=NULL?pAC:"close") + std::string(HTTP_ATTRIBUTE_ENDL));
    for (std::deque<Replay::Cookie>::const_iterator cit = m_Replay.m_Cookies.begin(); cit != m_Replay.m_Cookies.end(); ++cit) {
        const Replay::Cookie& c = *cit;
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&c.m_dtExpire));
        rConnect_.SendString(std::string("Set-Cookie: ") + c.m_sCookie + std::string("=") + c.m_sValue + std::string("; Expires=") + std::string(st) + std::string("; Domain=.") + c.m_sDomain +  std::string("; Path=/" HTTP_ATTRIBUTE_ENDL));
    }

    rConnect_.SendString(std::string(HTTP_ATTRIBUTE_CONTENT_TYPE HTTP_ATTRIBUTE_DIV) + m_Replay.m_sContentType);
    if (m_Replay.m_sContentTypeCP!=HTTP_CODEPAGE_NULL) {
        rConnect_.SendString(std::string("; charset=") + m_Replay.m_sContentTypeCP);
    }
    rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);

    if (m_Replay.m_sContentDisposition.size()!=0) {
        rConnect_.SendString(std::string(HTTP_ATTRIBUTE_CONTENT_DISPOSITION HTTP_ATTRIBUTE_DIV) + m_Replay.m_sContentDisposition);
        rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    }

    if (nLength!=-1) {
        rConnect_.SendString(HTTP_ATTRIBUTE_CONTENT_LENGTH ": ");
        rConnect_.SendNumber(nLength);
        rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    }

    if (m_Replay.m_tLastModify!=0) {
        rConnect_.SendString(HTTP_ATTRIBUTE_LAST_MODIFIED HTTP_ATTRIBUTE_DIV);
        strftime(st, sizeof(st), "%a, %d-%b-%Y %H:%M:%S GMT", gmtime(&m_Replay.m_tLastModify));
        rConnect_.SendString(st);
        rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    }
    rConnect_.SendString(HTTP_ATTRIBUTE_ENDL, sizeof(HTTP_ATTRIBUTE_ENDL)-1);
    if (psResponse_!=NULL) {
        rConnect_.SendString(psResponse_, nLength);
    }
}

void HTTPTextProtocolHeader::SetCookie(const std::string& sCookie_, const std::string& sValue_, time_t dtExpire_, const std::string& sDomain_) {
    m_Replay.m_Cookies.push_back(Replay::Cookie(sCookie_, sValue_, dtExpire_, sDomain_));
}

#ifdef WIN32
#define DIRECTORY_DIV   '\\'
static bool IsSlash (char ch_) { return ch_=='/'; }
#else
#define DIRECTORY_DIV   '/'
#endif


bool HTTPFileTransfer::SendFile(Network::Connection& rConnect_, HTTPTextProtocolHeader& rHTTP_, const std::string& sDocRoot_, SessionPtr sp_, const std::string& sFileName_) {
    const char* pQueryURI = rHTTP_.GetQueryURI();
    // TODO сделать корректировку по абсолютно-относительным переходам
    std::string     sFileName;
    if(sFileName_=="") {
        sFileName = sDocRoot_ + (pQueryURI + 1);
    }
    else {
        sFileName = sDocRoot_ + sFileName_;
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

    try {
        if(sFileName.length()>=MAX_PATH)
            throw std::string("Path is too long");

        char        bufCanonicFileName[MAX_PATH];
        memset(bufCanonicFileName, 0, sizeof(bufCanonicFileName));

#ifdef WIN32
        if(!PathCanonicalize((LPSTR)bufCanonicFileName, sFileName.c_str()))
            throw std::string("PathCanonicalize failed");
#else
        if(!realpath(sFileName.c_str(), bufCanonicFileName))
            throw std::string("realpath failed");
#endif

        sCanonicFileName=bufCanonicFileName;

        if(sCanonicFileName.substr(0, sDocRoot_.length())!=sDocRoot_)
            throw std::string("Access outside of docroot attempted");

        System::FastFile    ff;
        if (ff.Open(sCanonicFileName, -1, true)) {
            static struct SECT{
                const char* m_Ext;
                const char* m_CT;
                const char* m_CP;
            }   s_SECT[] = {
                {"js", HTTP_ATTRIBUTE_CONTENT_TYPE__APP_JS, HTTP_CODEPAGE_UTF8}
                , {"html", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8}
                , {"htm", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_HTML, HTTP_CODEPAGE_UTF8}
                , {"css", HTTP_ATTRIBUTE_CONTENT_TYPE__TEXT_CSS, HTTP_CODEPAGE_UTF8}
                , {"gif", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_GIF, HTTP_CODEPAGE_NULL}
                , {"jpg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL}
                , {"jpeg", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_JPG, HTTP_CODEPAGE_NULL}
                , {"png", HTTP_ATTRIBUTE_CONTENT_TYPE__IMAGE_PNG, HTTP_CODEPAGE_NULL}
                , {NULL, NULL, NULL}
            };

            const char* pIMS = rHTTP_.GetHeaderParameter("If-Modified-Since");
            if (pIMS!=NULL) {
                tm t;
                strptime(pIMS, "%a, %d-%b-%Y %H:%M:%S GMT", &t);
#ifdef WIN32
                time_t tt = _mkgmtime(&t);
#else
                time_t tt = timegm(&t);
#endif  // WIN32
                if (tt>=ff.GetTimeUpdate()) {
                    LOG_CONTEXT(LOG_CTX_NETWORK, LOG_LEVEL_DEBUG, "Dont send file '%s' length = %d. Response 304 (If-Modified-Since: '%s')", pQueryURI, ff.GetLength(), pIMS);
                    rHTTP_.SetResponseStatus(304, "Not Modified");
                    rHTTP_.Response(rConnect_, NULL, -1);
                    return true;
                }
                tt =tt;
            }

            const char*   pExt = sCanonicFileName.c_str();
            size_t  nExt = sCanonicFileName.size();
            for (;nExt>0;--nExt) {
                if (pExt[nExt]=='.') {
                    nExt++;
                    break;
                } else if (pExt[nExt]=='\\' || pExt[nExt]=='/') {
                    nExt = 0;
                    break;
                }
            }

            LOG_CONTEXT(LOG_CTX_NETWORK, LOG_LEVEL_DEBUG, "Send file '%s' length = %d", pQueryURI, ff.GetLength());
            if (nExt>0) {
                pExt += nExt;
                for (int k=0; s_SECT[k].m_Ext!=NULL; ++k) {
                    if (strcmp(s_SECT[k].m_Ext, pExt)==0) {
                        rHTTP_.SetContentType(s_SECT[k].m_CT, s_SECT[k].m_CP);
                        break;
                    }
                }
            }
            rHTTP_.SetLastModify(ff.GetTimeUpdate());
            rHTTP_.Response(rConnect_, NULL, ff.GetLength());
            rConnect_.SendData(reinterpret_cast<const boost::uint8_t*>(ff.GetMemory()), ff.GetLength());

            ff.Close();
        } else {
            throw std::string("File not found '%s'");
        }
    }
    catch(std::string err) {
        LOG_CONTEXT(LOG_CTX_NETWORK, LOG_LEVEL_ERROR, err.c_str(), sCanonicFileName.c_str());

        rHTTP_.SetResponseStatus(404, "Not found");
        rHTTP_.Response(rConnect_, "File not found", -1);

        return false;
    }

    return true;
}

int HTTPTextProtocolHeader::GetParameter(const std::string& sKey_, int nDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist?atoi(pValue):nDefault_;
}

const char* HTTPTextProtocolHeader::GetParameter(const std::string& sKey_, const char* sDefault_) const {
    bool bExist;
    const char* pValue = GetParameter(sKey_, &bExist);
    return bExist?pValue:sDefault_;
}

void HTTPTextProtocolHeader::CalculateHostPort() {
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

const char* HTTPTextProtocolHeader::GetHost() {
    if (m_nHost==-1) {
        CalculateHostPort();
    }
    return m_nHost==-1?"":m_abHeader.GetPosition(m_nHost);
}

const char* HTTPTextProtocolHeader::GetPort() {
    if (m_nHost==-1) {
        CalculateHostPort();
    }
    return (m_nPort==-1 || m_nPort==0)?"":m_abHeader.GetPosition(m_nPort);
}

const char* HTTPTextProtocolHeader::GetClientProxyIP() {
    size_t  idx = GetHeaderIndex(HTTP_ATTRIBUTE_X_FORWARDED_FOR);
    if (idx==-1) {
        idx = GetHeaderIndex(HTTP_ATTRIBUTE_X_REAL_IP);
        if (idx==-1) {
            return NULL;
        }
    }
    return m_abHeader.GetPosition(idx);
}
