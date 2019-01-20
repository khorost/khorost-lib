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
#include "app/khl-define.h"
#include "../../../server/sig_server.h"

using namespace khorost::db;

void db_pool::prepare(int nCount_, const std::string& sConnectParam_) {
    std::lock_guard<std::mutex> locker(m_mutex);

    for (auto n = 0; n < nCount_; ++n) {
        m_FreePool.emplace(boost::make_shared<db_connection_pool>(sConnectParam_));
    }
}

db_connection_pool_ptr db_pool::get_connection_pool() {
    std::unique_lock<std::mutex> locker(m_mutex);

    while (m_FreePool.empty()) {
        m_condition.wait(locker);
    }

    auto conn = m_FreePool.front();
    m_FreePool.pop();

    if (m_FreePool.size() < 3) {
        auto logger = spdlog::get(KHL_LOGGER_COMMON);
        logger->debug("[get from pool] size = {:d}", m_FreePool.size());
    }

    return conn;
}

void db_pool::release_connection_pool(db_connection_pool_ptr pdbc_) {
    std::unique_lock<std::mutex> locker(m_mutex);

    m_FreePool.push(pdbc_);
    locker.unlock();
    m_condition.notify_one();

    if (m_FreePool.size() < 4) {
        auto logger = spdlog::get(KHL_LOGGER_COMMON);
        logger->debug("[return to pool] size = {:d}", m_FreePool.size());
    }
}

std::string postgres::get_connect_param() const {
    std::string sc;
    sc += " host = " + m_sHost;
    sc += " port = " + std::to_string(m_nPort);
    sc += " dbname = " + m_sDatabase;
    sc += " user = " + m_sLogin;
    sc += " password = " + m_sPassword;

    return sc;
}

bool db_connection_pool::check_connect() {
    static int times = 0;
    try {
        times++;
        if (!m_spConnect->is_open()) {
            m_spConnect->activate();
        }
        times = 0;
        // hack
        pqxx::nontransaction txn(get_handle());

        auto r = txn.exec(
            "SELECT app "
            " FROM admin.khl_version "
            " LIMIT 1 ");
    } catch (const pqxx::broken_connection&) {
        if (times > 10) {
            times = 0;
            return false;
        }
        check_connect();
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

void postgres::ExecuteCustomSQL(bool bReadOnly_, const std::string& sSQL_, Json::Value& jvResult_) {
    db_connection conn(*this);

    if (bReadOnly_) {
        pqxx::read_transaction txn(conn.get_handle());
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
    } else {
        pqxx::work txn(conn.get_handle());
        sExecuteCustomSQL(txn, sSQL_, jvResult_);
        txn.commit();
    }
}

std::string linked_postgres::to_string(const pqxx::transaction_base& txn,
                                       const boost::posix_time::ptime& timestamp, const bool nullable_infinity) {
    if (timestamp.is_pos_infinity()) {
        return nullable_infinity ? "null" : "'infinity'::timestamp";
    } else if (timestamp.is_neg_infinity()) {
        return nullable_infinity ? "null" : "'-infinity'::timestamp";
    } else {
        return "'" + to_iso_extended_string(timestamp) + "+00'";
    }
}

std::string linked_postgres::to_string(const pqxx::transaction_base& txn, const Json::Value& info) {
    return info.isNull() ? "null" : (txn.quote(data::json_string(info)) + "::jsonb");
}

std::string linked_postgres::to_string(const pqxx::transaction_base& txn, const bool value) {
    return value ? "true" : "false";
}

std::string linked_postgres::to_string(const pqxx::transaction_base& txn, const unsigned int value) {
    return std::to_string(value);
}

bool khl_postgres::refresh_token(khorost::network::token_ptr& token, int access_timeout, int refresh_timeout, int append_time) const {
    db_connection conn(m_db_);
    pqxx::work txn(conn.get_handle());

    std::string access_interval, refresh_interval;
    const auto s_append_time = std::to_string(append_time);

    if (access_timeout != 0) {
        access_interval = "'" + std::to_string(access_timeout + append_time) + " SECONDS'";
    } else {
        access_interval = "'1 SECONDS' * ((kt.payload#>>'{delta_access_time}')::int + " + s_append_time + ")";
    }

    if (refresh_timeout != 0) {
        refresh_interval = "'" + std::to_string(refresh_timeout + append_time) + " SECONDS'";
    } else {
        refresh_interval = "'1 SECONDS' * ((kt.payload#>>'{delta_refresh_time}')::int + " + s_append_time + ")";
    }

    const auto pre_refresh_token = token->get_refresh_token();
    const auto r = txn.exec(
        "INSERT INTO admin.khl_tokens "
        " (access_token, access_time, refresh_token, refresh_time, payload) "
        " SELECT uuid_generate_v4(), NOW() + INTERVAL " + access_interval + " , uuid_generate_v4(), NOW() + INTERVAL " + refresh_interval + ", kt.payload "
        "  FROM admin.khl_tokens AS kt "
        " WHERE kt.refresh_token = '" + pre_refresh_token + "' "
        " RETURNING replace(access_token::text,'-',''), access_time AT TIME ZONE 'UTC', replace(refresh_token::text,'-',''), refresh_time AT TIME ZONE 'UTC'"
    );

    if (!r.empty()) {
        const auto& row0 = r[0];

        token->set_access_token(row0[0].as<std::string>());
        token->set_access_expire(boost::posix_time::time_from_string(row0[1].as<std::string>()));
        token->set_refresh_token(row0[2].as<std::string>());
        token->set_refresh_expire(boost::posix_time::time_from_string(row0[3].as<std::string>()));

        txn.exec(
            "UPDATE admin.khl_tokens "
            " SET access_time = NOW(), refresh_time = NOW() "
            " WHERE refresh_token = '" + pre_refresh_token + "' "
        );
    }

    txn.commit();

    return true;
}

khorost::network::token_ptr khl_postgres::create_token(const int access_timeout, const int refresh_timeout, int append_time,
                                                       const Json::Value& payload) const {
    db_connection conn(m_db_);
    pqxx::work txn(conn.get_handle());

    auto token_payload = payload;
    token_payload["delta_access_time"] = access_timeout;
    token_payload["delta_refresh_time"] = refresh_timeout;

    const auto r = txn.exec(
        "INSERT INTO admin.khl_tokens "
        " (access_token, access_time, refresh_token, refresh_time, payload) "
        " VALUES( uuid_generate_v4(), NOW() + INTERVAL '" + std::to_string(access_timeout + append_time) +
        " SECONDS', uuid_generate_v4(), NOW() + INTERVAL '" + std::to_string(refresh_timeout + append_time) + " SECONDS', " +
        to_string(txn, token_payload) + ") "
        " RETURNING replace(access_token::text,'-',''), access_time AT TIME ZONE 'UTC', replace(refresh_token::text,'-',''), refresh_time AT TIME ZONE 'UTC'"
    );
    txn.commit();

    if (r.empty()) {
        return nullptr;
    }

    const auto& row0 = r[0];
    return boost::make_shared<network::token>(row0[0].as<std::string>(),
                                              boost::posix_time::time_from_string(row0[1].as<std::string>()),
                                              row0[2].as<std::string>(),
                                              boost::posix_time::time_from_string(row0[3].as<std::string>()), token_payload);
}

khorost::network::token_ptr khl_postgres::load_token(const bool is_access_token, const std::string& token_id) const {
    db_connection conn(m_db_);
    pqxx::read_transaction txn(conn.get_handle());

    const auto code_token = std::string(is_access_token ? "access_token" : "refresh_token");

    auto r = txn.exec(
        "SELECT "
        "  replace(kt.access_token::text,'-','') , " // [0]
        "  kt.access_time AT TIME ZONE 'UTC' , " // [1]
        "  replace(kt.refresh_token::text,'-','') , " // [2]
        "  kt.refresh_time AT TIME ZONE 'UTC', " // [3]
        "  kt.payload " // [4]
        " FROM "
        "   admin.khl_tokens AS kt "
        " WHERE kt." + code_token + " = '" + token_id + "' "
    );

    if (r.empty()) {
        return nullptr;
    }

    Json::Value payload;

    const auto& row0 = r[0];

    if (!row0[4].is_null()) {
        data::parse_json_string(row0[4].as<std::string>(), payload);
    }

    return boost::make_shared<network::token>(row0[0].as<std::string>(),
                                              boost::posix_time::time_from_string(row0[1].as<std::string>()),
                                              row0[2].as<std::string>(),
                                              boost::posix_time::time_from_string(row0[3].as<std::string>()), payload);
}
