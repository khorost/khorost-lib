#include <queue>
#include <string.h>

#include "net/mail.h"
#include "util/logger.h"

using namespace khorost::network;

static std::queue<std::string> init_message(const std::string& to_, const std::string& from_,
                                            const std::string& subject_, const std::string& message_) {
    std::queue<std::string> msg;
    std::string s = "To: " + to_ + "\n";
    msg.push(s);
    s = "From: " + from_ + "\n";
    msg.push(s);
    s = "Subject: " + subject_ + "\n";
    msg.push(s);
    s = "\n";
    msg.push(s);
    msg.push(message_);

    return msg;
}

static size_t payload_source(void* ptr, size_t size, size_t nmemb, void* userp) {
    std::queue<std::string>* msg = static_cast<std::queue<std::string>*>(userp);
    std::string s;

    if (msg->empty()) {
        return 0;
    } else {
        s = msg->front();
        msg->pop();
    }

    memcpy(ptr, s.c_str(), s.size());
    return s.size();
}

void smtp::register_connect(const std::string& sHost_, const std::string& sUser_, const std::string& sPassword_) {
    m_sHost = sHost_;
    m_sUser = sUser_;
    m_sPassword = sPassword_;
}

bool smtp::send_message(const std::string& to_, const std::string& from_, const std::string& subject_,
                        const std::string& body_) {
    struct curl_slist* recipients = nullptr;

    CURL* curl = curl_easy_init();

    std::queue<std::string> msg = init_message(to_, from_, subject_, body_);

    if (curl != nullptr) {
        /* Set username and password */
        curl_easy_setopt(curl, CURLOPT_USERNAME, m_sUser.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, m_sPassword.c_str());
        /* This is the URL for your mailserver. Note the use of smtps:// rather
         * than smtp:// to request a SSL based connection. */
        curl_easy_setopt(curl, CURLOPT_URL, m_sHost.c_str());
        /* If you want to connect to a site who isn't using a certificate that is
        * signed by one of the certs in the CA bundle you have, you can skip the
        * verification of the server's certificate. This makes the connection
        * A LOT LESS SECURE.
        *
        * If you have a CA cert for the server stored someplace else than in the
        * default bundle, then the CURLOPT_CAPATH option might come handy for
        * you. */
#ifdef SKIP_PEER_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
        /* If the site you're connecting to uses a different host name that what
        * they have mentioned in their server certificate's commonName (or
        * subjectAltName) fields, libcurl will refuse to connect. You can skip
        * this check, but this will make the connection less secure. */
#ifdef SKIP_HOSTNAME_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
        /* Note that this option isn't strictly required, omitting it will result
        * in libcurl sending the MAIL FROM command with empty sender data. All
        * autoresponses should have an empty reverse-path, and should be directed
        * to the address in the reverse-path which triggered them. Otherwise,
        * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
        * details.
        */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from_.c_str());
        /* Add two recipients, in this particular case they correspond to the
        * To: and Cc: addressees in the header, but they could be any kind of
        * recipient. */
        recipients = curl_slist_append(recipients, to_.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        /* We're using a callback function to specify the payload (the headers and
        * body of the message). You could just use the CURLOPT_READDATA option to
        * specify a FILE pointer to read from. */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &msg);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        /* Since the traffic will be encrypted, it is very useful to turn on debug
        * information within libcurl to see what is happening during the
        * transfer */
//        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG(WARNING) << "SMTP error - " << curl_easy_strerror(res);
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }

    return true;
}
