// database.cpp : connection, prepared statements and transactions on top of libmysqlclient.
#include "database.h"
#include <mysql/mysql.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>

struct Database::MysqlHandle { MYSQL* m = nullptr; };

namespace {

// The C API uses `bool` (MySQL 8) or `my_bool` (older / MariaDB) for flags; use whichever the header says.
using MyBool = std::remove_pointer<decltype(MYSQL_BIND::is_null)>::type;

const char* envOr(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// Show the statement with its values filled in. Display only: the real statement still uses bound parameters.
std::string renderSql(const std::string& sql, const Params& params) {
    std::string out;
    size_t next = 0;
    for (char c : sql) {
        if (c == '?' && next < params.size()) {
            const Param& p = params[next++];
            if (p.isNull) out += "NULL";
            else if (p.secret) out += "'***'";
            else if (p.isBlob) out += "<BLOB " + std::to_string(p.text.size()) + " bytes>";
            else if (p.isNum) out += p.text;
            else {
                out += '\'';
                for (char ch : p.text) { if (ch == '\'' || ch == '\\') out += '\\'; out += ch; }
                out += '\'';
            }
        } else {
            out += c;
        }
    }
    return out;
}

SqlEntry baseEntry(const std::string& shownSql) {
    SqlEntry e;
    e.sql = shownSql;
    e.kind = classifySql(shownSql);
    return e;
}

// Open and configure one connection (TLS with certificate check when a CA file is given).
MYSQL* openConnection(const DbConfig& cfg, std::string& error) {
    MYSQL* m = mysql_init(nullptr);
    if (!m) { error = "mysql_init failed"; return nullptr; }
    mysql_options(m, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    unsigned timeout = 10;
    mysql_options(m, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    if (!cfg.sslCa.empty()) {                       // Aiven: encrypted connection, certificate checked
        mysql_options(m, MYSQL_OPT_SSL_CA, cfg.sslCa.c_str());
#if defined(MARIADB_VERSION_ID)
        MyBool verify = 1;
        mysql_options(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
#else
        unsigned mode = SSL_MODE_VERIFY_CA;
        mysql_options(m, MYSQL_OPT_SSL_MODE, &mode);
#endif
    }
    if (!mysql_real_connect(m, cfg.host.c_str(), cfg.user.c_str(), cfg.password.c_str(),
                            cfg.name.c_str(), cfg.port, nullptr, CLIENT_FOUND_ROWS)) {
        error = mysql_error(m);
        mysql_close(m);
        return nullptr;
    }
    return m;
}

}  // namespace

// 2006 = server has gone away, 2013 = lost connection during query, 2055 = lost connection (extended),
// 4031 = the server closed the connection because of inactivity (wait_timeout; MySQL 8.0.24 and newer, so also Aiven)
bool isConnectionLost(unsigned errNo) { return errNo == 2006 || errNo == 2013 || errNo == 2055 || errNo == 4031; }

// ---- parameters ----
Param P(int v) { return Param{false, true, std::to_string(v)}; }
Param P(long long v) { return Param{false, true, std::to_string(v)}; }
Param P(double v) { char b[64]; std::snprintf(b, sizeof b, "%.2f", v); return Param{false, true, b}; }
Param P(const std::string& v) { return Param{false, false, v}; }
Param P(const char* v) { return Param{false, false, v}; }
Param PNull() { return Param{true, false, ""}; }
Param PBlob(const std::string& bytes) { Param p{false, false, bytes}; p.isBlob = true; return p; }
Param PSecret(const std::string& v) { Param p{false, false, v}; p.secret = true; return p; }

DbConfig DbConfig::fromEnv() {
    DbConfig c;
    c.host = envOr("DB_HOST", "127.0.0.1");
    c.port = static_cast<unsigned>(std::atoi(envOr("DB_PORT", "3306")));
    c.user = envOr("DB_USER", "root");
    c.password = envOr("DB_PASSWORD", "");
    c.name = envOr("DB_NAME", "ims");
    c.sslCa = envOr("DB_SSL_CA", "");
    return c;
}

Database::~Database() {
    if (conn_) { mysql_close(conn_->m); delete conn_; }
}

bool Database::connect(const DbConfig& cfg, std::string& error) {
    MYSQL* m = openConnection(cfg, error);
    if (!m) return false;
    cfg_ = cfg;
    conn_ = new MysqlHandle{m};
    return true;
}

// The server may close an idle connection, restart for maintenance or the network may blink.
// Open a new connection with the same settings; a failure leaves the old handle in place.
bool Database::reconnect() {
    std::string err;
    MYSQL* m = openConnection(cfg_, err);
    if (!m) return false;
    if (conn_) { mysql_close(conn_->m); conn_->m = m; }
    else conn_ = new MysqlHandle{m};
    inTx_ = false;
    return true;
}

// Run one statement. If the connection was lost and we are NOT inside a transaction, reconnect and try once more:
// nothing was half done. Inside a transaction the error is passed on, because the server already rolled it back
// and repeating one statement alone would run it outside the transaction.
QueryResult Database::execute(const std::string& sql, const Params& params, SqlLog* log) {
    try {
        return executeOnce(sql, params, log);
    } catch (const DbError& e) {
        if (!isConnectionLost(e.errNo) || inTx_ || !reconnect()) throw;
        return executeOnce(sql, params, log);
    }
}

QueryResult Database::executeOnce(const std::string& sql, const Params& params, SqlLog* log) {
    if (!conn_) throw DbError(0, "Not connected to the database");
    auto t0 = std::chrono::steady_clock::now();
    SqlEntry entry = baseEntry(renderSql(sql, params));

    MYSQL_STMT* st = mysql_stmt_init(conn_->m);
    if (!st) throw DbError(0, "Out of memory");
    struct Closer { MYSQL_STMT* s; ~Closer() { mysql_stmt_close(s); } } closer{st};

    // Record a failure in the log and throw.
    auto fail = [&](unsigned no, const std::string& msg) {
        entry.errNo = no ? no : 1;
        entry.error = msg;
        entry.ms = msSince(t0);
        if (log) log->add(entry);
        throw DbError(no ? no : 1, msg);
    };

    if (mysql_stmt_prepare(st, sql.c_str(), static_cast<unsigned long>(sql.size()))) {
        fail(mysql_stmt_errno(st), mysql_stmt_error(st));
    }
    if (mysql_stmt_param_count(st) != params.size()) fail(0, "Wrong number of parameters for statement");

    // Bind parameters (every value is sent as text; MySQL converts it to the column type).
    std::vector<MYSQL_BIND> in(params.size());
    std::vector<unsigned long> inLen(params.size());
    auto inNull = std::make_unique<MyBool[]>(params.size() + 1);   // not vector<bool>: we need real pointers
    std::memset(in.data(), 0, in.size() * sizeof(MYSQL_BIND));
    for (size_t i = 0; i < params.size(); ++i) {
        inNull[i] = params[i].isNull ? 1 : 0;
        inLen[i] = static_cast<unsigned long>(params[i].text.size());
        in[i].buffer_type = params[i].isBlob ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING;
        in[i].buffer = const_cast<char*>(params[i].text.data());
        in[i].buffer_length = inLen[i];
        in[i].length = &inLen[i];
        in[i].is_null = &inNull[i];
    }
    if (!params.empty() && mysql_stmt_bind_param(st, in.data())) fail(mysql_stmt_errno(st), mysql_stmt_error(st));
    if (mysql_stmt_execute(st)) fail(mysql_stmt_errno(st), mysql_stmt_error(st));

    QueryResult res;
    MYSQL_RES* meta = mysql_stmt_result_metadata(st);
    if (meta) {                                           // statement returns rows (SELECT)
        unsigned ncols = mysql_num_fields(meta);
        MYSQL_FIELD* fields = mysql_fetch_fields(meta);
        for (unsigned i = 0; i < ncols; ++i) {
            res.columns.push_back(fields[i].name);
            res.numeric.push_back(IS_NUM(fields[i].type) ? 1 : 0);
        }
        if (mysql_stmt_store_result(st)) { mysql_free_result(meta); fail(mysql_stmt_errno(st), mysql_stmt_error(st)); }

        std::vector<std::vector<char>> bufs(ncols, std::vector<char>(256));
        std::vector<unsigned long> len(ncols);
        auto isNull = std::make_unique<MyBool[]>(ncols);
        auto err = std::make_unique<MyBool[]>(ncols);
        std::vector<MYSQL_BIND> out(ncols);
        std::memset(out.data(), 0, out.size() * sizeof(MYSQL_BIND));
        for (unsigned i = 0; i < ncols; ++i) {
            out[i].buffer_type = MYSQL_TYPE_STRING;
            out[i].buffer = bufs[i].data();
            out[i].buffer_length = bufs[i].size();
            out[i].length = &len[i];
            out[i].is_null = &isNull[i];
            out[i].error = &err[i];
        }
        if (mysql_stmt_bind_result(st, out.data())) { mysql_free_result(meta); fail(mysql_stmt_errno(st), mysql_stmt_error(st)); }

        int rc;
        while ((rc = mysql_stmt_fetch(st)) == 0 || rc == MYSQL_DATA_TRUNCATED) {
            std::vector<std::optional<std::string>> row;
            for (unsigned i = 0; i < ncols; ++i) {
                if (isNull[i]) { row.push_back(std::nullopt); continue; }
                if (len[i] > bufs[i].size()) {            // value longer than the buffer: fetch it whole
                    std::vector<char> big(len[i]);
                    MYSQL_BIND b;
                    std::memset(&b, 0, sizeof b);
                    b.buffer_type = MYSQL_TYPE_STRING;
                    b.buffer = big.data();
                    b.buffer_length = big.size();
                    unsigned long l2 = 0;
                    b.length = &l2;
                    mysql_stmt_fetch_column(st, &b, i, 0);
                    row.push_back(std::string(big.data(), l2));
                } else {
                    row.push_back(std::string(bufs[i].data(), len[i]));
                }
            }
            res.rows.push_back(std::move(row));
        }
        mysql_free_result(meta);
        if (rc == 1) fail(mysql_stmt_errno(st), mysql_stmt_error(st));
        entry.hasResult = true;
        entry.result.columns = res.columns;
        entry.result.total = res.rows.size();
        for (size_t i = 0; i < res.rows.size() && i < 5; ++i) entry.result.rows.push_back(res.rows[i]);
    } else {                                              // INSERT / UPDATE / DELETE
        res.affectedRows = static_cast<long long>(mysql_stmt_affected_rows(st));
        res.insertId = mysql_stmt_insert_id(st);
        entry.rowsAffected = res.affectedRows;
    }
    entry.ms = msSince(t0);
    if (log) log->add(entry);
    return res;
}

void Database::simpleOnce(const char* sql, SqlLog* log) {
    if (!conn_) throw DbError(0, "Not connected to the database");
    auto t0 = std::chrono::steady_clock::now();
    SqlEntry entry = baseEntry(sql);
    bool bad = mysql_real_query(conn_->m, sql, static_cast<unsigned long>(std::strlen(sql))) != 0;
    if (bad) { entry.errNo = mysql_errno(conn_->m); entry.error = mysql_error(conn_->m); }
    entry.ms = msSince(t0);
    if (log) log->add(entry);
    if (bad) throw DbError(entry.errNo, entry.error);
}

// Same repair rule as execute(): only outside a transaction.
void Database::simple(const char* sql, SqlLog* log) {
    try {
        simpleOnce(sql, log);
    } catch (const DbError& e) {
        if (!isConnectionLost(e.errNo) || inTx_ || !reconnect()) throw;
        simpleOnce(sql, log);
    }
}

void Database::begin(SqlLog* log) { simple("START TRANSACTION", log); inTx_ = true; }

void Database::commit(SqlLog* log) {
    try { simple("COMMIT", log); } catch (...) { inTx_ = false; throw; }
    inTx_ = false;
}

void Database::rollback(SqlLog* log) {
    try { simple("ROLLBACK", log); } catch (...) { inTx_ = false; throw; }
    inTx_ = false;
}
