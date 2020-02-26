#include "db/sqlite3.h"

#include <string.h>
#include <vector>

#pragma comment(lib,"sqlite3.lib")

using namespace khorost::db;

bool khl_sqlite3::Open(const std::string& sPathToDatabase_) {
    // TODO возможно здесь для русских букв в имени класса нужно будет конвертировать в utf8
    m_iResult = sqlite3_open(sPathToDatabase_.c_str(), &m_HandleDB);

    return m_HandleDB != NULL
        && RegistryExtentions()
        && PrepareDatabase();
}

bool khl_sqlite3::Close() {
    if (m_HandleDB != NULL) {
        m_iResult = sqlite3_close(m_HandleDB);
        m_HandleDB = NULL;
    }
    return true;
}

bool khl_sqlite3::IsTableExist(const std::string& sTable_) {
    char** azResult;
    int nrow, ncolumn;
    bool bExist = false;

    char* strSQL = sqlite3_mprintf(
        "SELECT name"
        " FROM SQLITE_MASTER"
        " WHERE type= 'table'"
        " AND   name= '%q'"
        " UNION"
        " SELECT name"
        " FROM SQLITE_TEMP_MASTER"
        " WHERE type= 'table'"
        " AND   name= '%q';", sTable_.c_str(), sTable_.c_str());

    m_iResult = sqlite3_get_table(m_HandleDB, strSQL, &azResult, &nrow, &ncolumn, &m_ErrorMessage);

    if (m_iResult == SQLITE_OK && nrow > 0) {
        bExist = true;
    }

    sqlite3_free_table(azResult);
    sqlite3_free(strSQL);

    return bExist;
}

bool khl_sqlite3::ExecuteStmt(const std::string& sql) {
    m_iResult = sqlite3_exec(m_HandleDB, sql.c_str(), NULL, NULL, &m_ErrorMessage);
    if (!SQLITE_SUCCESS(m_iResult)) {
        //        LogStrVA(LOG_DEBUG, "ExecuteStmt failed %d %s\nSQL:\n%s", m_iResult, m_ErrorMessage, sql.c_str());
        return false;
    }
    return true;
}

bool khl_sqlite3::PrepareDatabase() {
    sqlite3_enable_load_extension(m_HandleDB, 1);

    ExecuteStmt("SELECT load_extension('sqlite3_ext')");

    return true;
}

bool khl_sqlite3::RegistryExtentions() {
    return true;
}

bool khl_sqlite3::ugTransactionBegin() {
    return (m_iResult = sqlite3_exec(m_HandleDB, "BEGIN",NULL,NULL, &m_ErrorMessage)) == SQLITE_OK;
}

bool khl_sqlite3::ugTransactionCommit() {
    return (m_iResult = sqlite3_exec(m_HandleDB, "COMMIT",NULL,NULL, &m_ErrorMessage)) == SQLITE_OK;
}

bool khl_sqlite3::ugTransactionRollback() {
    return (m_iResult = sqlite3_exec(m_HandleDB, "ROLLBACK",NULL,NULL, &m_ErrorMessage)) == SQLITE_OK;
}

khl_sqlite3::Adapter::Adapter(DBHandle hDb_)
    : m_HandleDB(hDb_), sqstmt(NULL) {
}

khl_sqlite3::Adapter::~Adapter() {
    Cleanup();
}

void khl_sqlite3::Adapter::DumpSqliteError(int nResult) {
    const char* lpcError = sqlite3_errmsg(m_HandleDB);
    if (lpcError) {
        //        LogStrVA(LOG_DEBUG, "SQLITE: retvalue [%d] message [%s]\nSQL:\n%s", nResult, lpcError, m_sPreparedSql.c_str());
    } else {
        //        LogStrVA(LOG_DEBUG, "SQLITE: retvalue [%d] (empty message)\nSQL:\n%s", nResult, m_sPreparedSql.c_str());
    }
}

bool khl_sqlite3::Adapter::Prepare(const std::string& sql_) {
    m_sPreparedSql = sql_;
    Cleanup();

    const char* tail = nullptr;
    const auto result = sqlite3_prepare_v2(m_HandleDB, sql_.c_str(), int(sql_.size()), &sqstmt, &tail);
    if (!SQLITE_SUCCESS(result)) {
        DumpSqliteError(result);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParam(int column, int value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParam(int column, unsigned int value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParam(int column, bool value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParam(int column, double value) {
    int nResult = sqlite3_bind_double(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParamString(int column, const char* value, int size) {
    const auto result = value == nullptr
                            ? sqlite3_bind_null(sqstmt, column)
                            : sqlite3_bind_text(sqstmt, column, value, size == -1 ? int(strlen(value)) : size,
                                                SQLITE_STATIC);

    if (!SQLITE_SUCCESS(result)) {
        DumpSqliteError(result);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParam(int column, const std::string& value) {
    int nResult = sqlite3_bind_text(sqstmt, column, value.c_str(), int(value.size()), SQLITE_STATIC);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::bind_param(const int column, const std::vector<uint8_t>& data) {
    const auto result = sqlite3_bind_blob(sqstmt, column, &data[0], data.size(), SQLITE_STATIC);
    if (!SQLITE_SUCCESS(result)) {
        DumpSqliteError(result);
    }
    return true;
}

bool khl_sqlite3::Adapter::BindParamNull(int column) {
    int nResult = sqlite3_bind_null(sqstmt, column);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool khl_sqlite3::Adapter::GetValue(int column, int& value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = sqlite3_column_int(sqstmt, column);
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::GetValue(int column, unsigned int& value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = sqlite3_column_int(sqstmt, column);
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::GetValue(int column, bool& value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = (sqlite3_column_int(sqstmt, column) == 1);
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::GetValue(int column, float& value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = (float)sqlite3_column_double(sqstmt, column);
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::GetValue(int column, std::string& value) {
    column--;
    const char* lpcText = NULL;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        lpcText = (const char *)sqlite3_column_text(sqstmt, column);
        if (lpcText != NULL) {
            value = lpcText;
        }
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::get_value(int column, const char*& data, size_t& data_size) const {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        data_size = sqlite3_column_bytes(sqstmt, column);
        data = static_cast<const char *>(sqlite3_column_blob(sqstmt, column));
        return true;
    }
    return false;
}

bool khl_sqlite3::Adapter::IsNull(int column) {
    return sqlite3_column_type(sqstmt, column - 1) == SQLITE_NULL;
}

bool khl_sqlite3::Adapter::Cleanup() {
    if (sqstmt != NULL) {
        int nResult = sqlite3_finalize(sqstmt);
        sqstmt = NULL;
    }
    return true;
}


khl_sqlite3::Inserter::Inserter(khl_sqlite3::DBHandle hDb)
    : Adapter(hDb) {
}

khl_sqlite3::Inserter::~Inserter() {
}

bool khl_sqlite3::Inserter::Exec() {
    int nResult = sqlite3_step(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
        return false;
    }
    sqlite3_reset(sqstmt);
    return true;
}

bool khl_sqlite3::Inserter::PrepareAndExecute(const std::string& sql) {
    if (Prepare(sql))
        return Exec();
    return false;
}

int khl_sqlite3::Inserter::GetChanges() {
    return sqlite3_changes(m_HandleDB);
}

int khl_sqlite3::Inserter::GetTotalChanges() {
    return sqlite3_total_changes(m_HandleDB);
}

khl_sqlite3::Reader::Reader(khl_sqlite3::DBHandle hDb)
    : Adapter(hDb) {
}

khl_sqlite3::Reader::~Reader() {
}

bool khl_sqlite3::Reader::MoveFirst() {
    int nResult = sqlite3_reset(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
    }
    return MoveNext();
}

bool khl_sqlite3::Reader::MoveNext() {
    int nResult = sqlite3_step(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
    }
    return (nResult == SQLITE_ROW);
}
