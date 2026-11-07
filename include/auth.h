// auth.h : password hashing and login sessions.
#pragma once
#include <ctime>
#include <map>
#include <mutex>
#include <string>

// "salt$sha256hex(salt + password)": the format stored in Users.password_hash.
std::string makeSalt();                                                  // 16 random hex characters
std::string hashPassword(const std::string& password, const std::string& salt);
bool verifyPassword(const std::string& password, const std::string& stored);   // constant-time compare
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
private:
    std::mutex mu_;
    std::map<std::string, Session> sessions_;
};
