// sqllog.cpp : collects statements and turns them into JSON or text.
#include "sqllog.h"
#include <cctype>
#include <sstream>

SqlEntry& SqlLog::add(SqlEntry e) {
    e.seq = static_cast<int>(entries_.size()) + 1;
    entries_.push_back(std::move(e));
    return entries_.back();
}

double SqlLog::totalMs() const {
    double t = 0;
    for (const SqlEntry& e : entries_) t += e.ms;
    return t;
}

std::string classifySql(const std::string& sql) {
    size_t i = 0;
    while (i < sql.size() && std::isspace(static_cast<unsigned char>(sql[i]))) ++i;
    std::string w;
    while (i < sql.size() && std::isalpha(static_cast<unsigned char>(sql[i]))) {
        w += static_cast<char>(std::toupper(static_cast<unsigned char>(sql[i++])));
    }
    if (w == "SELECT" || w == "SHOW" || w == "EXPLAIN") return "DQL";
    if (w == "INSERT" || w == "UPDATE" || w == "DELETE") return "DML";
    if (w == "START" || w == "BEGIN" || w == "COMMIT" || w == "ROLLBACK" || w == "SAVEPOINT") return "TCL";
    if (w == "GRANT" || w == "REVOKE") return "DCL";
    return "DDL";
}

static nlohmann::json snapshotJson(const SqlSnapshot& s) {
    if (!s.present) return nullptr;
    return {{"table", s.table}, {"key", s.key}, {"row", s.row}};
}

nlohmann::json SqlLog::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const SqlEntry& e : entries_) {
        nlohmann::json j;
        j["seq"] = e.seq;
        j["sql"] = e.sql;
        j["kind"] = e.kind;
        j["rows_affected"] = e.rowsAffected < 0 ? nlohmann::json(nullptr) : nlohmann::json(e.rowsAffected);
        if (e.hasResult) {
            nlohmann::json rows = nlohmann::json::array();
            for (const auto& r : e.result.rows) {
                nlohmann::json row = nlohmann::json::array();
                for (const auto& c : r) row.push_back(c ? nlohmann::json(*c) : nlohmann::json(nullptr));
                rows.push_back(row);
            }
            j["result"] = {{"columns", e.result.columns}, {"rows", rows}, {"total", e.result.total}};
        } else {
            j["result"] = nullptr;
        }
        nlohmann::json trg = nlohmann::json::array();
        for (const SqlTriggerNote& t : e.triggers) trg.push_back({{"name", t.name}, {"note", t.note}, {"level", t.level}});
        j["triggers"] = trg;
        j["before"] = snapshotJson(e.before);
        j["after"] = snapshotJson(e.after);
        if (e.errNo) j["error"] = {{"mysql_errno", e.errNo}, {"message", e.error}};
        else j["error"] = nullptr;
        j["ms"] = e.ms;
        arr.push_back(j);
    }
    return arr;
}

std::string SqlLog::toText() const {
    std::ostringstream o;
    for (const SqlEntry& e : entries_) {
        o << "  " << e.seq << ". " << e.sql << "   [" << e.kind << ", " << e.ms << " ms]\n";
        if (e.rowsAffected >= 0) o << "       -> " << e.rowsAffected << " row(s) affected\n";
        if (e.hasResult) {
            for (const auto& r : e.result.rows) {
                o << "       -> ";
                for (size_t c = 0; c < r.size(); ++c) o << (c ? " | " : "") << (r[c] ? *r[c] : "NULL");
                o << "\n";
            }
            if (e.result.total > e.result.rows.size()) o << "       -> (" << e.result.total << " rows in all)\n";
        }
        for (const SqlTriggerNote& t : e.triggers) o << "       trigger " << t.name << " [" << t.level << "]: " << t.note << "\n";
        if (e.before.present) {
            o << "       before " << e.before.table << " " << e.before.key << ":";
            for (const auto& kv : e.before.row) o << " " << kv.first << "=" << kv.second;
            o << "\n";
        }
        if (e.after.present) {
            o << "       after  " << e.after.table << " " << e.after.key << ":";
            for (const auto& kv : e.after.row) o << " " << kv.first << "=" << kv.second;
            o << "\n";
        }
        if (e.errNo) o << "       ERROR " << e.errNo << ": " << e.error << "\n";
    }
    return o.str();
}
