// auth_tests.cpp : password hashing and sessions. Reference hashes were computed independently with Python's hashlib
// (sha256 and pbkdf2_hmac), so this also proves the C++ code and the load script (python3) agree.
#include <iostream>
#include "auth.h"

static int failures = 0, checks = 0;
#define CHECK(cond, label) do { ++checks; if (cond) std::cout << "  PASS  " << label << "\n"; \
    else { ++failures; std::cout << "  FAIL  " << label << "   (" #cond ")\n"; } } while (0)

int main() {
    const std::string pw = "Admin@2026";
    const std::string legacy = "0123456789abcdef$2175f59802233bb94572e470cdab235fa34be3045e669f3e3af2d8c52a0a88ac";            // old format: salt$sha256(salt+password)
    const std::string pb100k = "pbkdf2-sha256$100000$fedcba9876543210$ec9f5f51e8bb9305960c1fe1e7e01540dd383d584db7e5b64224f18af4c97926";
    const std::string pb1k = "pbkdf2-sha256$1000$fedcba9876543210$4f0e15b7698c457f964af973074130088fa03a8b08bb1082853892ffa0677834";                // valid, but fewer rounds than today

    std::cout << "[PBKDF2 hashes]\n";
    CHECK(verifyPassword(pw, pb100k), "Python-made PBKDF2 hash (100000 rounds) verifies in C++");
    CHECK(!verifyPassword("Admin@2027", pb100k) && !verifyPassword("", pb100k), "wrong and empty passwords are refused");
    CHECK(verifyPassword(pw, pb1k), "a PBKDF2 hash with fewer rounds still verifies");
    std::string mine = hashPassword(pw, "00112233aabbccdd");
    CHECK(mine.compare(0, 14, "pbkdf2-sha256$") == 0 && verifyPassword(pw, mine) && !verifyPassword(pw + "x", mine), "hashPassword output verifies its own password only");
    CHECK(hashPassword(pw, "aaaaaaaaaaaaaaaa") != hashPassword(pw, "bbbbbbbbbbbbbbbb"), "different salts give different hashes");
    CHECK(mine.size() < 140, "the stored text fits the password_hash column (140)");
    CHECK(makeSalt().size() == 16 && makeSalt() != makeSalt(), "salts are 16 hex characters and not repeated");

    std::cout << "[old SHA-256 format is still accepted]\n";
    CHECK(verifyPassword(pw, legacy) && !verifyPassword("nope", legacy), "old-format hash verifies");
    CHECK(needsRehash(legacy), "an old-format hash is marked for replacement");
    CHECK(needsRehash(pb1k), "a PBKDF2 hash with fewer rounds is marked for replacement");
    CHECK(!needsRehash(pb100k) && !needsRehash(mine), "a current hash is left alone");

    std::cout << "[malformed stored values never verify]\n";
    for (const char* bad : {"", "$", "abc", "pbkdf2-sha256$", "pbkdf2-sha256$100000", "pbkdf2-sha256$100000$salt", "pbkdf2-sha256$x$salt$00",
                          "pbkdf2-sha256$0$salt$0000000000000000000000000000000000000000000000000000000000000000",
                          "pbkdf2-sha256$100000$$0000000000000000000000000000000000000000000000000000000000000000",
                          "pbkdf2$100000$salt$00", "0000000000000000$short"}) {
        CHECK(!verifyPassword(pw, bad), std::string("refused: '") + bad + "'");
    }

    std::cout << "[sessions]\n";
    SessionStore s;
    std::string t1 = s.create(1, "a", "ADMIN"), t2 = s.create(1, "a", "ADMIN"), t3 = s.create(2, "b", "EMPLOYEE");
    Session out;
    CHECK(t1.size() == 64 && t1 != t2 && s.find(t1, out) && out.userId == 1 && out.role == "ADMIN", "tokens are random, 64 characters, and find the user");
    s.destroyUser(1, t2);
    CHECK(!s.find(t1, out) && s.find(t2, out) && s.find(t3, out), "destroyUser ends the user's sessions except the one kept, others untouched");
    s.destroy(t2);
    CHECK(!s.find(t2, out) && !s.find("not-a-token", out), "logout and unknown tokens");

    std::cout << "[login throttle]\n";
    LoginThrottle t;
    int wait = 0;
    for (int i = 0; i < 4; ++i) t.failure("u:x", 5);
    CHECK(!t.blocked("u:x", wait), "4 failures: not blocked yet");
    t.failure("u:x", 5);
    CHECK(t.blocked("u:x", wait) && wait > 0 && wait <= 60, "5th failure blocks for up to a minute");
    CHECK(!t.blocked("u:y", wait), "other keys are not affected");
    t.success("u:x");
    CHECK(!t.blocked("u:x", wait), "a correct login clears the key");

    std::cout << "\n" << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
