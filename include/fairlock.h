// fairlock.h : a mutex that serves threads strictly in the order they asked (a ticket lock).
// std::mutex gives no such promise, so with several browsers at once a later request could overtake an earlier one.
// The server uses this lock around every request, which makes "first come, first served" true for sales as well.
#pragma once
#include <condition_variable>
#include <mutex>

class FairMutex {
public:
    void lock() {
        std::unique_lock<std::mutex> g(m_);
        const unsigned long mine = nextTicket_++;                 // take a ticket ...
        cv_.wait(g, [&] { return serving_ == mine; });            // ... and wait until it is called
    }
    void unlock() {
        std::lock_guard<std::mutex> g(m_);
        ++serving_;
        cv_.notify_all();
    }
private:
    std::mutex m_;
    std::condition_variable cv_;
    unsigned long nextTicket_ = 0, serving_ = 0;
};
