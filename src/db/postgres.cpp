#include "db/postgres.h"

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib, "libpqxxD")
#else
#pragma comment(lib, "libpqxx")
#endif
#endif // WIN32

using namespace DB;

bool Postgres::Reconnect() {
    std::string sc;
    sc += " host = " + m_sHost;
    sc += " port = " + std::to_string(m_nPort);
    sc += " dbname = " + m_sDatabase;
    sc += " user = " + m_sLogin;
    sc += " password = " + m_sPassword;

    m_DBConnection.reset(new pqxx::connection(sc));
    return true;
}

bool Postgres::Disconnect() {
    m_DBConnection.reset(NULL);
    return true;
}

bool Postgres::CheckConnect() {
    static int times = 0;
    try {
        times++;
        if (!m_DBConnection->is_open()) {
            m_DBConnection->activate();
        }
        times = 0;
        // hack
        pqxx::read_transaction txn(*m_DBConnection);

        pqxx::result r = txn.exec(
            "SELECT user_id "
            " FROM users "
            " LIMIT 1 ");
    } catch (const pqxx::broken_connection &) {
        if (times > 10) {
            times = 0;
            return false;
        }
        CheckConnect();
    }
    return true;
}
