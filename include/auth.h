// auth.h : password hashing and login sessions.
#pragma once
#include <ctime>
#include <map>
#include <mutex>
#include <string>

// Users.password_hash holds "pbkdf2-sha256$<iterations>$<salt>$<hex>". The older format "salt$sha256hex(salt+password)"
// is still accepted and replaced by the new one the next time that user signs in (see needsRehash).
std::string makeSalt();                                                  // 16 random hex characters
std::string hashPassword(const std::string& password, const std::string& salt);   // new format, kPbkdf2Iterations rounds
bool verifyPassword(const std::string& password, const std::string& stored);      // either format, constant-time compare
bool needsRehash(const std::string& stored);                             // old format, or fewer rounds than today
constexpr int kPbkdf2Iterations = 100000;
std::string randomToken();                                               // 64 random hex characters

struct Session {
    int userId = 0;
    std::string username;
    std::string role;               // "ADMIN" or "EMPLOYEE"
    std::time_t expires = 0;
};

// Server-side sessions: the browser only holds a random token in an HTTP-only cookie.
class SessionStore {
public:
    static constexpr int kLifetimeSeconds = 8 * 60 * 60;                 // 8 hours
    std::string create(int userId, const std::string& username, const std::string& role);
    bool find(const std::string& token, Session& out);                   // false if unknown or expired
    void destroy(const std::string& token);
    // End every session of one user (account deleted, role or password changed); keepToken stays logged in.
    void destroyUser(int userId, const std::string& keepToken = "");
private:
    std::mutex mu_;
    std::map<std::string, Session> sessions_;
};

// Slows down password guessing. Too many failures for one key (a username or a client address) block that key
// for a minute. Keys are added by the caller, e.g. "u:admin" and "i:203.0.113.5".
class LoginThrottle {
public:
    static constexpr int kWindowSeconds = 15 * 60;      // failures older than this are forgotten
    static constexpr int kBlockSeconds = 60;
    bool blocked(const std::string& key, int& secondsLeft);
    void failure(const std::string& key, int maxFailures);
    void success(const std::string& key);                // a correct password clears the key
private:
    struct Entry { int fails = 0; std::time_t first = 0; std::time_t blockedUntil = 0; };
    std::mutex mu_;
    std::map<std::string, Entry> entries_;
};
