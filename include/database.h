// database.h : thin wrapper over the MySQL C API (libmysqlclient).
// Every statement is a prepared statement with bound parameters: values are never pasted into SQL text.
#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "sqllog.h"

struct DbConfig {
    std::string host = "127.0.0.1";
    unsigned port = 3306;
    std::string user = "root";
    std::string password;
    std::string name = "ims";
    std::string sslCa;                  // path to CA certificate; empty = no certificate check (local use)
    static DbConfig fromEnv();          // DB_HOST DB_PORT DB_USER DB_PASSWORD DB_NAME DB_SSL_CA
};

// One statement parameter.
struct Param {
    bool isNull = false;
    bool isNum = false;                 // only affects how the value is shown in the SQL log
    std::string text;
    bool isBlob = false;                // binary value (photo), sent as BLOB
    bool secret = false;                // shown as '***' in the SQL log (password hashes)
};
Param P(int v);
Param P(long long v);
Param P(double v);                      // formatted with 2 decimals (money)
Param P(const std::string& v);
Param P(const char* v);
Param PNull();
Param PBlob(const std::string& bytes);
Param PSecret(const std::string& v);
using Params = std::vector<Param>;

struct DbError : std::runtime_error {
    unsigned errNo;
    DbError(unsigned n, const std::string& msg) : std::runtime_error(msg), errNo(n) {}
};

struct QueryResult {
    std::vector<std::string> columns;
    std::vector<char> numeric;                                   // 1 = column has a numeric type (for JSON output)
    std::vector<std::vector<std::optional<std::string>>> rows;   // every value as text, NULL = nullopt
    long long affectedRows = 0;
    unsigned long long insertId = 0;
};

class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool connect(const DbConfig& cfg, std::string& error);
    bool connected() const { return conn_ != nullptr; }

    // Run one prepared statement. Appends an entry to `log` (if given), also on failure. Throws DbError.
    QueryResult execute(const std::string& sql, const Params& params, SqlLog* log);

    // Transaction control (simple protocol). Each call is logged.
    void begin(SqlLog* log);
    void commit(SqlLog* log);
    void rollback(SqlLog* log);

private:
    void simple(const char* sql, SqlLog* log);
    struct MysqlHandle;                 // keeps <mysql.h> out of this header
    MysqlHandle* conn_ = nullptr;
};
