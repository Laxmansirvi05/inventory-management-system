// routes_auth.cpp : POST /api/auth/login, POST /api/auth/logout, GET /api/auth/me
#include <cctype>
#include "server.h"

namespace {

// Same cost as a real check, so an unknown user name cannot be told apart by the time the answer takes.
const char* kDummyHash = "pbkdf2-sha256$100000$0000000000000000$0000000000000000000000000000000000000000000000000000000000000000";

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
    // Too many wrong guesses for this user name or from this address: refuse before touching the database.
    std::string lowered = username.substr(0, 64);
    for (char& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const std::string userKey = "u:" + lowered, ipKey = "i:" + c.remoteAddr;
    int wait = 0;
    if (c.app.throttle.blocked(userKey, wait) || c.app.throttle.blocked(ipKey, wait)) {
        return errReply(429, "TOO_MANY_ATTEMPTS", "Too many failed sign-in attempts. Try again in " + std::to_string(wait) + " seconds.");
    }
    // Not logged: the result row contains the password hash.
    QueryResult r = c.app.db.execute("SELECT user_id, password_hash, role FROM Users WHERE username = ?", {P(username)}, nullptr);
    bool found = !r.rows.empty();
    bool ok = verifyPassword(password, found ? *r.rows[0][1] : kDummyHash);   // same work for unknown users
    if (!found || !ok) {
        c.app.throttle.failure(userKey, 5);          // 5 wrong passwords for one user name ...
        c.app.throttle.failure(ipKey, 30);           // ... or 30 from one address block it for a minute
        return errReply(401, "INVALID_CREDENTIALS", "Invalid username or password");
    }
    c.app.throttle.success(userKey);

    int userId = std::stoi(*r.rows[0][0]);
    std::string role = *r.rows[0][2];
    if (needsRehash(*r.rows[0][1])) {                // old hash format or fewer rounds: store a fresh PBKDF2 hash now
        SqlLog ignored;                              // never shown to the client (it holds a password hash)
        runStatement(c.app.db, "UPDATE Users SET password_hash = ? WHERE user_id = ?",
                     {PSecret(hashPassword(password, makeSalt())), P(userId)}, ignored);   // a failure here must not block the login
    }
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

// GET /api/health : process is up and products are loaded. No database call (Render polls it and it must not wake a
// sleeping database). GET /api/health?deep=1 also runs SELECT 1, to prove the database answers (used by scripts/warmup.sh).
Reply health(Ctx& c) {
    json out = {{"status", "up"}, {"products", c.app.inv.size()}};
    if (c.q("deep") == "1") {
        try {
            c.app.db.execute("SELECT 1", {}, nullptr);
            out["database"] = "ok";
        } catch (const DbError& e) {
            return errReply(503, "DATABASE_UNAVAILABLE", e.what(), e.errNo);
        }
    }
    return okReply(out);
}

}  // namespace

void registerAuthRoutes(App& app) {
    app.add("GET", "/api/health", Access::Public, health);
    app.add("POST", "/api/auth/login", Access::Public, login);
    app.add("POST", "/api/auth/logout", Access::Public, logout);
    app.add("GET", "/api/auth/me", Access::Any, me);
}
