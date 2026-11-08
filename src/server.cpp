// server.cpp : cpp-httplib glue: session check, role check, one-request-at-a-time lock,
// JSON envelope {ok, data, error, sql_log}, static files from public/.
#include "server.h"
#include <cstdio>
#include <ctime>
#include <httplib.h>

// ---------- helpers ----------

std::string Ctx::q(const std::string& key, const std::string& fallback) const {
    auto it = query.find(key);
    return it == query.end() ? fallback : it->second;
}

int Ctx::paramInt(size_t i) const {
    if (i >= params.size() || params[i].empty() || params[i].size() > 9) return -1;
    return std::stoi(params[i]);
}

Reply okReply(json data, int status) {
    Reply r;
    r.status = status;
    r.data = std::move(data);
    return r;
}

Reply errReply(int status, const std::string& code, const std::string& message, unsigned mysqlErrno) {
    Reply r;
    r.ok = false;
    r.status = status;
    r.code = code;
    r.message = message;
    r.mysqlErrno = mysqlErrno;
    return r;
}

// Turn a MySQL error number (or our own message with number 0) into an HTTP status and a short code.
static Reply dbErrorReply(unsigned no, const std::string& msg) {
    switch (no) {
        case 1062: return errReply(409, "DUPLICATE", msg, no);
        case 1451: return errReply(409, "IN_USE", msg, no);
        case 1452: return errReply(400, "INVALID_REFERENCE", msg, no);
        case 1644: return errReply(409, "INSUFFICIENT_STOCK", msg, no);
        case 3819: case 4025: return errReply(400, "CONSTRAINT", msg, no);
        case 1406: return errReply(400, "TOO_LONG", msg, no);
        case 0:
            if (msg.find("not found") != std::string::npos) return errReply(404, "NOT_FOUND", msg);
            return errReply(400, "BAD_REQUEST", msg);
        default:   return errReply(400, "DB_ERROR", msg, no);
    }
}

Reply writeFailure(const WriteResult& w) { return dbErrorReply(w.errNo, w.error); }

json cellToJson(const std::optional<std::string>& v, bool numeric) {
    if (!v) return nullptr;
    if (numeric) {
        try {
            if (v->find_first_of(".eE") != std::string::npos) return std::stod(*v);
            return std::stoll(*v);
        } catch (...) {}
    }
    return *v;
}

json rowsToJson(const QueryResult& r) {
    json arr = json::array();
    for (const auto& row : r.rows) {
        json o = json::object();
        for (size_t c = 0; c < row.size() && c < r.columns.size(); ++c) {
            o[r.columns[c]] = cellToJson(row[c], c < r.numeric.size() && r.numeric[c]);
        }
        arr.push_back(o);
    }
    return arr;
}

bool jsonString(const json& b, const char* key, std::string& out) {
    if (!b.is_object() || !b.contains(key) || !b[key].is_string()) return false;
    out = b[key].get<std::string>();
    return true;
}

bool jsonInt(const json& b, const char* key, int& out) {
    if (!b.is_object() || !b.contains(key) || !b[key].is_number_integer()) return false;
    long long v = b[key].get<long long>();
    if (v < -2147483647LL || v > 2147483647LL) return false;
    out = static_cast<int>(v);
    return true;
}

bool jsonNumber(const json& b, const char* key, double& out) {
    if (!b.is_object() || !b.contains(key) || !b[key].is_number()) return false;
    out = b[key].get<double>();
    return true;
}

std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string firstOfMonth() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-01", std::localtime(&t));
    return buf;
}

void App::refreshNames() {
    categoryNames.clear();
    supplierNames.clear();
    QueryResult c = db.execute("SELECT category_id, category_name FROM Categories", {}, nullptr);
    for (const auto& r : c.rows) categoryNames[std::stoi(*r[0])] = *r[1];
    QueryResult s = db.execute("SELECT supplier_id, supplier_name FROM Suppliers", {}, nullptr);
    for (const auto& r : s.rows) supplierNames[std::stoi(*r[0])] = *r[1];
}

// ---------- request handling ----------

namespace {

const char* kCookieName = "ims_session";

std::string cookieValue(const httplib::Request& req, const std::string& name) {
    std::string header = req.get_header_value("Cookie");
    size_t pos = 0;
    while (pos < header.size()) {
        size_t end = header.find(';', pos);
        if (end == std::string::npos) end = header.size();
        std::string part = trimmed(header.substr(pos, end - pos));
        if (part.compare(0, name.size() + 1, name + "=") == 0) return part.substr(name.size() + 1);
        pos = end + 1;
    }
    return "";
}

void writeJson(httplib::Response& res, const Reply& r, const SqlLog& log) {
    json out;
    out["ok"] = r.ok;
    if (r.ok) {
        out["data"] = r.data;
    } else {
        out["data"] = nullptr;
        out["error"] = {{"code", r.code}, {"mysql_errno", r.mysqlErrno ? json(r.mysqlErrno) : json(nullptr)}, {"message", r.message}};
    }
    out["sql_log"] = log.toJson();
    res.status = r.status;
    res.set_content(out.dump(), "application/json");
}

void handle(App& app, const RouteDef& route, const httplib::Request& req, httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    Ctx ctx(app);
    SqlLog empty;

    // 1. who is calling, and are they allowed?
    ctx.token = cookieValue(req, kCookieName);
    Session session;
    bool loggedIn = !ctx.token.empty() && app.sessions.find(ctx.token, session);
    if (loggedIn) ctx.user = &session;
    if (route.access != Access::Public && !loggedIn) {
        writeJson(res, errReply(401, "UNAUTHENTICATED", "Please log in"), empty);
        return;
    }
    if (route.access == Access::Admin && session.role != "ADMIN") {
        writeJson(res, errReply(403, "FORBIDDEN", "Administrator access required"), empty);
        return;
    }

    // 2. read the request
    for (const auto& kv : req.params) ctx.query[kv.first] = kv.second;
    for (size_t i = 1; i < req.matches.size(); ++i) ctx.params.push_back(req.matches[i]);
    ctx.contentType = req.get_header_value("Content-Type");
    // Behind Render's proxy the last X-Forwarded-For entry is the address the proxy saw (earlier ones come from the client).
    std::string forwarded = req.get_header_value("X-Forwarded-For");
    size_t comma = forwarded.rfind(',');
    ctx.remoteAddr = forwarded.empty() ? req.remote_addr : trimmed(comma == std::string::npos ? forwarded : forwarded.substr(comma + 1));
    ctx.rawBody = req.body;
    if (ctx.contentType.compare(0, 16, "application/json") == 0 && !req.body.empty()) {
        ctx.body = json::parse(req.body, nullptr, false);
        if (ctx.body.is_discarded()) {
            writeJson(res, errReply(400, "INVALID_JSON", "Request body is not valid JSON"), empty);
            return;
        }
    }

    // 3. run it (one request at a time) and answer
    Reply reply;
    {
        std::lock_guard<std::mutex> lock(app.mu);
        try {
            reply = route.handler(ctx);
        } catch (const DbError& e) {
            reply = dbErrorReply(e.errNo, e.what());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "internal error on %s %s: %s\n", route.method.c_str(), req.path.c_str(), e.what());
            reply = errReply(500, "INTERNAL", "Internal server error");
        }
    }
    if (!reply.setCookie.empty()) res.set_header("Set-Cookie", reply.setCookie);
    if (!reply.rawType.empty()) {
        res.status = reply.status;
        res.set_content(reply.rawBody, reply.rawType);
        return;
    }
    writeJson(res, reply, ctx.log);
}

}  // namespace

int runServer(App& app, int port) {
    httplib::Server svr;
    svr.set_payload_max_length(2 * 1024 * 1024);           // photos are limited to 1 MB; leave headroom

    // Protective headers on EVERY response, pages and API alike. All scripts and styles come from this server;
    // style-src allows inline style attributes because the pages set a few.
    httplib::Headers secure = {
        {"X-Content-Type-Options", "nosniff"},
        {"X-Frame-Options", "DENY"},
        {"Referrer-Policy", "same-origin"},
        {"Permissions-Policy", "camera=(), microphone=(), geolocation=()"},
        {"Content-Security-Policy", "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; "
                                    "frame-ancestors 'none'; base-uri 'self'; form-action 'self'"}};
    if (app.secureCookie) secure.emplace("Strict-Transport-Security", "max-age=31536000");   // HTTPS deployments only
    svr.set_default_headers(secure);

    for (const RouteDef& route : app.routes) {
        auto cb = [&app, &route](const httplib::Request& req, httplib::Response& res) { handle(app, route, req, res); };
        if (route.method == "GET") svr.Get(route.pattern, cb);
        else if (route.method == "POST") svr.Post(route.pattern, cb);
        else if (route.method == "PUT") svr.Put(route.pattern, cb);
        else if (route.method == "DELETE") svr.Delete(route.pattern, cb);
    }

    // Errors raised by httplib itself (unknown path, wrong method, body too large ...) get the same JSON shape,
    // and keep their real status.
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        // httplib calls this for every 4xx/5xx, also for errors our routes already answered: leave those alone.
        if (req.path.compare(0, 5, "/api/") != 0 || !res.body.empty()) return;
        SqlLog empty;
        std::string code = "BAD_REQUEST", msg = "The request could not be processed";
        switch (res.status) {
            case 404: code = "NOT_FOUND"; msg = "No such API endpoint"; break;
            case 405: code = "METHOD_NOT_ALLOWED"; msg = "This endpoint does not accept that method"; break;
            case 411: code = "LENGTH_REQUIRED"; msg = "Content-Length is required"; break;
            case 413: code = "TOO_LARGE"; msg = "The request body is too large"; break;
            default: if (res.status >= 500) { code = "INTERNAL"; msg = "Internal server error"; }
        }
        writeJson(res, errReply(res.status, code, msg), empty);
    });

    if (!svr.set_mount_point("/", app.publicDir)) {
        std::fprintf(stderr, "warning: static folder '%s' not found; only /api is served\n", app.publicDir.c_str());
    }
    std::printf("IMS listening on http://0.0.0.0:%d (static files: %s)\n", port, app.publicDir.c_str());
    std::fflush(stdout);
    return svr.listen("0.0.0.0", port) ? 0 : 1;
}
