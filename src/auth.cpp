// auth.cpp : SHA-256 password hashing (OpenSSL) and an in-memory session table.
#include "auth.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdexcept>

namespace {

std::string toHex(const unsigned char* data, size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s += digits[data[i] >> 4]; s += digits[data[i] & 15]; }
    return s;
}

std::string randomHex(size_t bytes) {
    unsigned char buf[64];
    if (bytes > sizeof buf || RAND_bytes(buf, static_cast<int>(bytes)) != 1) throw std::runtime_error("random source failed");
    return toHex(buf, bytes);
}

std::string sha256Hex(const std::string& text) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_Digest(text.data(), text.size(), md, &len, EVP_sha256(), nullptr);
    return toHex(md, len);
}

}  // namespace

std::string makeSalt() { return randomHex(8); }
std::string randomToken() { return randomHex(32); }

std::string hashPassword(const std::string& password, const std::string& salt) {
    return salt + "$" + sha256Hex(salt + password);
}

bool verifyPassword(const std::string& password, const std::string& stored) {
    size_t dollar = stored.find('$');
    if (dollar == std::string::npos) return false;
    std::string expected = hashPassword(password, stored.substr(0, dollar));
    if (expected.size() != stored.size()) return false;
    return CRYPTO_memcmp(expected.data(), stored.data(), expected.size()) == 0;   // no early exit on mismatch
}

std::string SessionStore::create(int userId, const std::string& username, const std::string& role) {
    std::lock_guard<std::mutex> g(mu_);
    std::time_t now = std::time(nullptr);
    for (auto it = sessions_.begin(); it != sessions_.end();) {          // drop expired sessions
        if (it->second.expires <= now) it = sessions_.erase(it);
        else ++it;
    }
    std::string token = randomToken();
    sessions_[token] = Session{userId, username, role, now + kLifetimeSeconds};
    return token;
}

bool SessionStore::find(const std::string& token, Session& out) {
    std::lock_guard<std::mutex> g(mu_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    if (it->second.expires <= std::time(nullptr)) { sessions_.erase(it); return false; }
    out = it->second;
    return true;
}

void SessionStore::destroy(const std::string& token) {
    std::lock_guard<std::mutex> g(mu_);
    sessions_.erase(token);
}

void SessionStore::destroyUser(int userId, const std::string& keepToken) {
    std::lock_guard<std::mutex> g(mu_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.userId == userId && it->first != keepToken) it = sessions_.erase(it);
        else ++it;
    }
}

bool LoginThrottle::blocked(const std::string& key, int& secondsLeft) {
    std::lock_guard<std::mutex> g(mu_);
    auto it = entries_.find(key);
    std::time_t now = std::time(nullptr);
    if (it == entries_.end() || it->second.blockedUntil <= now) return false;
    secondsLeft = static_cast<int>(it->second.blockedUntil - now);
    return true;
}

void LoginThrottle::failure(const std::string& key, int maxFailures) {
    std::lock_guard<std::mutex> g(mu_);
    std::time_t now = std::time(nullptr);
    if (entries_.size() > 10000) entries_.clear();                      // bounded memory, also against random user names
    for (auto it = entries_.begin(); it != entries_.end();) {            // forget old, unblocked entries
        if (it->second.blockedUntil <= now && now - it->second.first > kWindowSeconds) it = entries_.erase(it);
        else ++it;
    }
    Entry& e = entries_[key];
    if (e.fails == 0 || now - e.first > kWindowSeconds) { e.fails = 0; e.first = now; }
    if (++e.fails >= maxFailures) { e.blockedUntil = now + kBlockSeconds; e.fails = 0; e.first = now; }
}

void LoginThrottle::success(const std::string& key) {
    std::lock_guard<std::mutex> g(mu_);
    entries_.erase(key);
}
