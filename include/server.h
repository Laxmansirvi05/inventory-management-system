// server.h : shared pieces for the REST routes. Only server.cpp talks to cpp-httplib.
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "auth.h"
#include "database.h"
#include "inventory.h"
#include "orders.h"
#include "repository.h"
#include "sqllog.h"

using json = nlohmann::json;

enum class Access { Public, Any, Admin };       // Any = every logged-in user

// What a route returns. The server wraps it as {ok, data, error, sql_log}.
struct Reply {
    int status = 200;
    bool ok = true;
    json data = nullptr;
    std::string code;                           // error code, e.g. FORBIDDEN, DUPLICATE
    std::string message;
    unsigned mysqlErrno = 0;
    std::string rawBody, rawType;               // set for non-JSON replies (product photo)
    std::string setCookie;                      // optional Set-Cookie header value
};

struct App;

// Everything a route handler needs about one request.
struct Ctx {
    App& app;
    const Session* user = nullptr;              // null on public routes without a session
    std::string token;                          // session cookie value, if any
    SqlLog log;                                 // statements run for this request
    json body;                                  // parsed JSON body (null if none)
    std::string rawBody, contentType;
    std::map<std::string, std::string> query;
    std::vector<std::string> params;            // regex groups from the URL
    explicit Ctx(App& a) : app(a) {}
    std::string q(const std::string& key, const std::string& fallback = "") const;
    int paramInt(size_t i) const;               // URL group i as a positive int, -1 if missing or too long
};

using Handler = std::function<Reply(Ctx&)>;
struct RouteDef { std::string method, pattern; Access access; Handler handler; };

struct OrderRecord {                            // recent sale orders for the FIFO queue card
    int orderId = 0;
    int customerId = 0;
    int itemCount = 0;
    std::string status;                         // pending, done, failed
    std::string message;
    long long saleId = 0;
    double total = 0.0;
};

struct App {
    Database db;
    Inventory inv;
    OrderQueue orders;
    SessionStore sessions;
    std::mutex mu;                              // one request at a time: single DB connection, shared memory
    std::map<int, std::string> categoryNames, supplierNames;
    std::deque<OrderRecord> orderHistory;       // newest last, capped
    int nextOrderId = 1;
    std::string publicDir = "public";
    std::string shopName = "Inventory Store";
    bool secureCookie = false;                  // add the Secure flag (set when served over HTTPS)
    std::vector<RouteDef> routes;
    void add(const std::string& method, const std::string& pattern, Access access, Handler h) {
        routes.push_back(RouteDef{method, pattern, access, std::move(h)});
    }
    void refreshNames();                        // reload category and supplier names used in product JSON
};

// ---- helpers shared by the route files (server.cpp) ----
Reply okReply(json data = json::object(), int status = 200);
Reply errReply(int status, const std::string& code, const std::string& message, unsigned mysqlErrno = 0);
Reply writeFailure(const WriteResult& w);                      // MySQL / validation failure -> HTTP error
json rowsToJson(const QueryResult& r);                         // array of {column: value} objects
json cellToJson(const std::optional<std::string>& v, bool numeric);
bool jsonString(const json& b, const char* key, std::string& out);
bool jsonInt(const json& b, const char* key, int& out);
bool jsonNumber(const json& b, const char* key, double& out);
std::string trimmed(const std::string& s);
std::string firstOfMonth();                                    // "YYYY-MM-01" for today's month

// ---- route registration (one function per file) ----
void registerAuthRoutes(App& app);
void registerCatalogRoutes(App& app);
void registerStockRoutes(App& app);
void registerReportRoutes(App& app);

int runServer(App& app, int port);                             // blocks
