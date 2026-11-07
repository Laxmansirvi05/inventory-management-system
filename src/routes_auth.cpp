// routes_auth.cpp : POST /api/auth/login, POST /api/auth/logout, GET /api/auth/me
#include "server.h"

namespace {

const char* kDummyHash = "0000000000000000$0000000000000000000000000000000000000000000000000000000000000000";

std::string sessionCookie(const App& app, const std::string& token, int maxAge) {
    return "ims_session=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" + std::to_string(maxAge) +
           (app.secureCookie ? "; Secure" : "");
}

Reply login(Ctx& c) {
    std::string username, password;
    if (!jsonString(c.body, "username", username) || !jsonString(c.body, "password", password) ||
        username.empty() || password.empty()) {
        return errReply(400, "BAD_REQUEST", "Username and password are required");
    }
    // Not logged: the result row contains the password hash.
    QueryResult r = c.app.db.execute("SELECT user_id, password_hash, role FROM Users WHERE username = ?", {P(username)}, nullptr);
    bool found = !r.rows.empty();
    bool ok = verifyPassword(password, found ? *r.rows[0][1] : kDummyHash);   // same work for unknown users
    if (!found || !ok) return errReply(401, "INVALID_CREDENTIALS", "Invalid username or password");

    int userId = std::stoi(*r.rows[0][0]);
    std::string role = *r.rows[0][2];
    std::string token = c.app.sessions.create(userId, username, role);
    Reply rep = okReply({{"user_id", userId}, {"username", username}, {"role", role}});
    rep.setCookie = sessionCookie(c.app, token, SessionStore::kLifetimeSeconds);
    return rep;
}

Reply logout(Ctx& c) {
    if (!c.token.empty()) c.app.sessions.destroy(c.token);
    Reply rep = okReply();
    rep.setCookie = sessionCookie(c.app, "", 0);
    return rep;
}

Reply me(Ctx& c) {
    return okReply({{"user_id", c.user->userId}, {"username", c.user->username}, {"role", c.user->role}});
}

// GET /api/health : process is up and products are loaded (no database call, so it never wakes a sleeping database)
Reply health(Ctx& c) { return okReply({{"status", "up"}, {"products", c.app.inv.size()}}); }

}  // namespace

void registerAuthRoutes(App& app) {
    app.add("GET", "/api/health", Access::Public, health);
    app.add("POST", "/api/auth/login", Access::Public, login);
    app.add("POST", "/api/auth/logout", Access::Public, logout);
    app.add("GET", "/api/auth/me", Access::Any, me);
}
