#include "db/postgres.h"

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib, "libpqxxD")
#else
#pragma comment(lib, "libpqxx")
#endif
#endif // WIN32

#include <iostream>
#include <boost/make_shared.hpp>

#include "util/logger.h"

using namespace khorost::DB;

void DBPool::Prepare(int nCount_, const std::string& sConnectParam_) {
    std::lock_guard<std::mutex> locker(m_mutex);

    for (auto n = 0; n < nCount_; ++n) {
        m_FreePool.emplace(boost::make_shared<DBConnectionPool>(sConnectParam_));
    }
}

DBConnectionPoolPtr DBPool::GetConnectionPool() {
    std::unique_lock<std::mutex> locker(m_mutex);

    while (m_FreePool.empty()) {
        m_condition.wait(locker);
    }

    auto conn = m_FreePool.front();
    m_FreePool.pop();

    if (m_FreePool.size() < 3) {
        LOG(DEBUG) << "[get from pool] size = " << m_FreePool.size();
    }

    return conn;
}

void DBPool::ReleaseConnectionPool(DBConnectionPoolPtr pdbc_) {
    std::unique_lock<std::mutex> locker(m_mutex);
    m_FreePool.push(pdbc_);
    locker.unlock();
    m_condition.notify_one();

    if (m_FreePool.size() < 4) {
        LOG(DEBUG) << "[return to pool] size = " << m_FreePool.size();
    }
}

std::string Postgres::GetConnectParam() const {
    std::string sc;
    sc += " host = " + m_sHost;
    sc += " port = " + std::to_string(m_nPort);
    sc += " dbname = " + m_sDatabase;
    sc += " user = " + m_sLogin;
    sc += " password = " + m_sPassword;

    return sc;
}

bool DBConnectionPool::CheckConnect() {
    static int times = 0;
    try {
        times++;
        if (!m_spConnect->is_open()) {
            m_spConnect->activate();
        }
        times = 0;
        // hack
        pqxx::nontransaction txn(GetHandle());

        auto r = txn.exec(
            "SELECT app "
            " FROM admin.khl_version "
            " LIMIT 1 ");
    } catch (const pqxx::broken_connection&) {
        if (times > 10) {
            times = 0;
            return false;
        }
        CheckConnect();
    }
    return true;
}

static void sExecuteCustomSQL(pqxx::transaction_base& txn_, const std::string& sSQL_, Json::Value& jvResult_) {
    try {
        auto r = txn_.exec(sSQL_);

        Json::Value jvHeader, jvTable;
        auto columns = r.columns();
        for (decltype(columns) c = 0; c < columns; ++c) {
            jvHeader.append(r.column_name(c));
        }

        for (auto i = r.begin(); i != r.end(); ++i) {
            Json::Value jvRow;

            auto isize = i->size();
            for (decltype(isize) f = 0; f < isize; ++f) {
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
    } catch (const pqxx::pqxx_exception& e) {
        Json::Value jvError;
        jvError["What"] = e.base().what();
        //        OutputDebugString(e.base().what());
        std::cerr << e.base().what() << std::endl;
        const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e.base());
        if (s != NULL) {
            jvError["Query"] = s->query();
            //            OutputDebugString(s->query().c_str());
            std::cerr << "Query was: " << s->query() << std::endl;
        }
        jvResult_["Error"] = jvError;
    }
}

void Postgres::ExecuteCustomSQL(bool bReadOnly_, const std::string& sSQL_, Json::Value& jvResult_) {
    DBConnection conn(*this);

    if (bReadOnly_) {
        pqxx::read_transaction txn(conn.GetHandle());
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
    } else {
        pqxx::work txn(conn.GetHandle());
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
        txn.commit();
    }
}

std::string LinkedPostgres::to_string(const pqxx::transaction_base& txn,
                                      const boost::posix_time::ptime& timestamp, const bool nullable_infinity) {
    if (timestamp.is_pos_infinity()) {
        return nullable_infinity ? "null" : "'infinity'::timestamp";
    } else if (timestamp.is_neg_infinity()) {
        return nullable_infinity ? "null" : "'-infinity'::timestamp";
    } else {
        return "'" + to_iso_extended_string(timestamp) + "+00'";
    }
}

std::string LinkedPostgres::to_string(const pqxx::transaction_base& txn, const Json::Value& info) {
    return info.isNull() ? "null" : (txn.quote(Json::FastWriter().write(info)) + "::jsonb");
}

std::string LinkedPostgres::to_string(const pqxx::transaction_base& txn, const bool value) {
    return value ? "true" : "false";
}

std::string LinkedPostgres::to_string(const pqxx::transaction_base& txn, const unsigned int value) {
    return std::to_string(value);
}
