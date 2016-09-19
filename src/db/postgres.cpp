#include "db/postgres.h"

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib, "libpqxxD")
#else
#pragma comment(lib, "libpqxx")
#endif
#endif // WIN32

#include <iostream>

using namespace khorost::DB;

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
            "SELECT id "
            " FROM admin.users "
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

static void sExecuteCustomSQL(pqxx::transaction_base& txn_, const std::string& sSQL_, Json::Value& jvResult_) {
    try{
        pqxx::result r = txn_.exec(sSQL_);

        Json::Value jvHeader, jvTable;
        for (pqxx::row::size_type c = 0; c < r.columns(); ++c) {
            jvHeader.append(r.column_name(c));
        }

        for (pqxx::result::const_iterator i = r.begin(); i != r.end(); i++) {
            Json::Value jvRow;
            for (pqxx::row::size_type f = 0; f < i->size(); ++f) {
                if (i[f].is_null()) {
                    jvRow[i[f].name()] = Json::nullValue;
                } else {
                    jvRow[i[f].name()] = i[f].c_str();
                }
            }

            jvTable.append(jvRow);
        }
        jvResult_["Header"] = jvHeader;
        jvResult_["Table"] = jvTable;
    } catch (const pqxx::pqxx_exception &e) {
        Json::Value jvError;
        jvError["What"] = e.base().what();
        OutputDebugString(e.base().what());
        std::cerr << e.base().what() << std::endl;
        const pqxx::sql_error *s = dynamic_cast<const pqxx::sql_error*>(&e.base());
        if (s!=NULL) {
            jvError["Query"] = s->query();
            OutputDebugString(s->query().c_str());
            std::cerr << "Query was: " << s->query() << std::endl;
        }
        jvResult_["Error"] = jvError;
    }
}

void Postgres::ExecuteCustomSQL(bool bReadOnly_, const std::string& sSQL_, Json::Value& jvResult_) {
    if (bReadOnly_) {
        pqxx::read_transaction txn(*m_DBConnection);
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
    } else {
        pqxx::work txn(*m_DBConnection);
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
        txn.commit();
    }
}

