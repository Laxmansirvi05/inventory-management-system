// sqllog.h : per-request log of every SQL statement, returned to the browser for the Live SQL panel.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct SqlTriggerNote {
    std::string name;
    std::string note;
    std::string level;      // "ok" (allowed), "action" (changed data), "error" (rejected)
};

struct SqlResultTable {
    std::vector<std::string> columns;
    std::vector<std::vector<std::optional<std::string>>> rows;   // at most 5 kept for display
    size_t total = 0;                                            // rows the statement really returned
};

struct SqlSnapshot {                // one row before or after a change (for the Before/After boxes)
    bool present = false;
    std::string table;
    std::string key;
    std::map<std::string, std::string> row;
};

struct SqlEntry {
    int seq = 0;
    std::string sql;                // text with the parameter values filled in (display only, never executed)
    std::string kind;               // DQL, DML, DDL, TCL, DCL
    long long rowsAffected = -1;    // -1 = not applicable (SELECT, TCL)
    bool hasResult = false;
    SqlResultTable result;
    std::vector<SqlTriggerNote> triggers;
    SqlSnapshot before, after;
    unsigned errNo = 0;             // 0 = success
    std::string error;
    double ms = 0;
};

class SqlLog {
public:
    SqlEntry& add(SqlEntry e);                  // assigns the sequence number
    SqlEntry& last() { return entries_.back(); }
    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }
    const std::vector<SqlEntry>& entries() const { return entries_; }
    double totalMs() const;
    nlohmann::json toJson() const;              // shape defined in docs/09-Frontend-Plan.md section 7
    std::string toText() const;                 // readable form for console output
private:
    std::vector<SqlEntry> entries_;
};

// DQL / DML / DDL / TCL / DCL from the first keyword of a statement.
std::string classifySql(const std::string& sql);
