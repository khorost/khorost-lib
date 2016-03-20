#include "db/sqlite3.h"

#pragma comment(lib,"sqlite3.lib")

using namespace khorost::DB;

SQLite3::SQLite3() {

}

SQLite3::~SQLite3() {

}

bool SQLite3::Open(const std::string& sPathToDatabase_){
    // TODO возможно здесь для русских букв в имени класса нужно будет конвертировать в utf8
    m_iResult = sqlite3_open(sPathToDatabase_.c_str(), &m_HandleDB);

    return m_HandleDB!=NULL 
        && RegistryExtentions()
        && PrepareDatabase();
} 

bool SQLite3::Close(){
    if(m_HandleDB!=NULL){
        m_iResult = sqlite3_close(m_HandleDB);
        m_HandleDB = NULL;
    }
    return true;
}

bool SQLite3::IsTableExist(const std::string& sTable_){
    char**			azResult;
    int				nrow, ncolumn;
    bool			bExist = false;

    char*			strSQL = sqlite3_mprintf(
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

    if (m_iResult==SQLITE_OK && nrow>0) {
        bExist = true;
    }

    sqlite3_free_table(azResult);
    sqlite3_free(strSQL);

    return bExist;
}

bool SQLite3::ExecuteStmt(const std::string &sql) {
    m_iResult = sqlite3_exec(m_HandleDB, sql.c_str(), NULL, NULL, &m_ErrorMessage);
    if (!SQLITE_SUCCESS(m_iResult)){
//        LogStrVA(LOG_DEBUG, "ExecuteStmt failed %d %s\nSQL:\n%s", m_iResult, m_ErrorMessage, sql.c_str());
        return false;
    }
    return true;
}

bool SQLite3::PrepareDatabase() {
    sqlite3_enable_load_extension(m_HandleDB, 1);

    ExecuteStmt("SELECT load_extension('sqlite3_ext')");

    return true;
}

bool SQLite3::RegistryExtentions(){
    return true;
}

bool SQLite3::ugTransactionBegin(){
    return (m_iResult = sqlite3_exec(m_HandleDB,"BEGIN",NULL,NULL,&m_ErrorMessage))==SQLITE_OK;
}

bool SQLite3::ugTransactionCommit(){
    return (m_iResult = sqlite3_exec(m_HandleDB,"COMMIT",NULL,NULL,&m_ErrorMessage))==SQLITE_OK;
}

bool SQLite3::ugTransactionRollback(){
    return (m_iResult = sqlite3_exec(m_HandleDB,"ROLLBACK",NULL,NULL,&m_ErrorMessage))==SQLITE_OK;
}

SQLite3::Adapter::Adapter(DBHandle hDb_)
: m_HandleDB(hDb_), sqstmt(NULL){
}

SQLite3::Adapter::~Adapter() {
    Cleanup();
}

void SQLite3::Adapter::DumpSqliteError(int nResult) {
    const char* lpcError = sqlite3_errmsg(m_HandleDB);
    if (lpcError){
//        LogStrVA(LOG_DEBUG, "SQLITE: retvalue [%d] message [%s]\nSQL:\n%s", nResult, lpcError, m_sPreparedSql.c_str());
    }else{
//        LogStrVA(LOG_DEBUG, "SQLITE: retvalue [%d] (empty message)\nSQL:\n%s", nResult, m_sPreparedSql.c_str());
    }
}

bool SQLite3::Adapter::Prepare(const std::string& sql_) {
    m_sPreparedSql = sql_;
    Cleanup();
    const char* pzTail = NULL;
    int nResult = sqlite3_prepare_v2(m_HandleDB, sql_.c_str(), sql_.size(), &sqstmt, &pzTail);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParam(int column, int value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParam(int column, unsigned int value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParam(int column, bool value) {
    int nResult = sqlite3_bind_int(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParam(int column, double value) {
    int nResult = sqlite3_bind_double(sqstmt, column, value);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParamString(int column, const char* value, int size) {
    int nResult;
    if(value==NULL){
        nResult = sqlite3_bind_null(sqstmt, column);
    }else{
        nResult = sqlite3_bind_text(sqstmt, column, value, size==-1?strlen(value):size, SQLITE_STATIC);
    }
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParam(int column, const std::string& value) {
    int nResult = sqlite3_bind_text(sqstmt, column, value.c_str(), value.size(), SQLITE_STATIC);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::BindParamNull(int column) {
    int nResult = sqlite3_bind_null(sqstmt, column);
    if (!SQLITE_SUCCESS(nResult)) {
        DumpSqliteError(nResult);
    }
    return true;
}

bool SQLite3::Adapter::GetValue(int column, int &value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = sqlite3_column_int(sqstmt, column);
        return true;
    }
    return false;
}

bool SQLite3::Adapter::GetValue(int column, unsigned int &value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = sqlite3_column_int(sqstmt, column);
        return true;
    }
    return false;
}

bool SQLite3::Adapter::GetValue(int column, bool &value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = (sqlite3_column_int(sqstmt, column) == 1);
        return true;
    }
    return false;
}

bool SQLite3::Adapter::GetValue(int column, float &value) {
    column--;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        value = (float)sqlite3_column_double(sqstmt, column);
        return true;
    }
    return false;
}

bool SQLite3::Adapter::GetValue(int column, std::string &value) {
    column--;
    const char* lpcText = NULL;
    if (sqlite3_column_type(sqstmt, column) != SQLITE_NULL) {
        lpcText = (const char *)sqlite3_column_text(sqstmt, column);
        if (lpcText!=NULL) {
            value = lpcText;
        }
        return true;
    }
    return false;
}

bool SQLite3::Adapter::IsNull(int column) {
    return sqlite3_column_type(sqstmt, column - 1) == SQLITE_NULL;
}

bool SQLite3::Adapter::Cleanup() {
    if(sqstmt!=NULL){
        int nResult = sqlite3_finalize(sqstmt);
        sqstmt = NULL;
    }
    return true;
}


SQLite3::Inserter::Inserter(SQLite3::DBHandle hDb)
    : Adapter(hDb)
{
}

SQLite3::Inserter::~Inserter()
{
}

bool SQLite3::Inserter::Exec() {
    int nResult = sqlite3_step(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
        return false;
    }
    sqlite3_reset(sqstmt);
    return true;
}

bool SQLite3::Inserter::PrepareAndExecute(const std::string& sql) {
    if (Prepare(sql))
        return Exec();
    return false;
}

int	SQLite3::Inserter::GetChanges() {
    return sqlite3_changes(m_HandleDB);
}
int	SQLite3::Inserter::GetTotalChanges() {
    return sqlite3_total_changes(m_HandleDB);
}

SQLite3::Reader::Reader(SQLite3::DBHandle hDb)
: Adapter(hDb)
{
}

SQLite3::Reader::~Reader()
{
}

bool SQLite3::Reader::MoveFirst() {
    int nResult = sqlite3_reset(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
    }
    return MoveNext();
}

bool SQLite3::Reader::MoveNext() {
    int nResult = sqlite3_step(sqstmt);
    if (!SQLITE_SUCCESSROW(nResult)) {
        DumpSqliteError(nResult);
    }
    return (nResult == SQLITE_ROW);
}
