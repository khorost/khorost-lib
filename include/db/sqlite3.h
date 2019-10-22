#pragma once

#include <string>

#include <sqlite3.h>

namespace khorost {
    namespace db {

#define SQLITE_SUCCESS(result) (result == SQLITE_OK || result == SQLITE_DONE)
#define SQLITE_SUCCESSROW(result) (SQLITE_SUCCESS(result) || result == SQLITE_ROW)

        class khl_sqlite3 {
            typedef sqlite3* DBHandle;

            DBHandle m_HandleDB; // 
            char* m_ErrorMessage; // 
            int m_iResult; // 
        public:
            khl_sqlite3() = default;
            virtual ~khl_sqlite3() = default;

            virtual bool Open(const std::string& sPathToDatabase_);
            virtual bool Close();
            virtual bool RegistryExtentions();
            virtual bool PrepareDatabase();

            DBHandle GetDB() { return m_HandleDB; }

            bool IsTableExist(const std::string& sTable_);

            bool ExecuteStmt(const std::string& sSQL_);

            bool ugTransactionBegin();
            bool ugTransactionCommit();
            bool ugTransactionRollback();

            class Adapter {
            protected:
                DBHandle m_HandleDB;
                sqlite3_stmt* sqstmt;
                std::string m_sPreparedSql;

                void DumpSqliteError(int nResult);

            public:
                Adapter(DBHandle hDB_);
                virtual ~Adapter();

                bool Prepare(const std::string& sql_);
                bool Cleanup();

                bool BindParam(int column_, int value);
                bool BindParam(int column_, unsigned int value);
                bool BindParam(int column_, const std::string& value);
                bool BindParamString(int column, const char* value, int size = -1);
                bool BindParam(int column, bool value);
                bool BindParam(int column, double value);
                bool BindParamNull(int column);

                bool GetValue(int column, int& value);
                bool GetValue(int column, unsigned int& value);
                bool GetValue(int column, bool& value);
                bool GetValue(int column, float& value);
                bool GetValue(int column, std::string& value);
                bool IsNull(int column);
            };

            class Inserter : public Adapter {
            public:
                Inserter(DBHandle hDb);
                virtual ~Inserter();

                bool Exec();
                bool PrepareAndExecute(const std::string& sql);

                int GetChanges();
                int GetTotalChanges();
            };

            class Reader : public Adapter {
            public:
                Reader(DBHandle hDb);
                virtual ~Reader();

                bool MoveFirst();
                bool MoveNext();
            };
        };
    }
}


