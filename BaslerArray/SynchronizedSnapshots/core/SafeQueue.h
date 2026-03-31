#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

// ========================= SAFE QUEUE =========================
template<typename T>
class SafeQueue {
    queue<T> q;
    mutex m;
    condition_variable cv;
    bool stopped = false;

public:
    void push(const T& item) {
        {
            lock_guard<mutex> lock(m);
            q.push(item);
        }
        cv.notify_one();
    }

    // Queue needs to be stopped in order to prevent consumer getting stuck when exiting
    void stop() {
        {
            lock_guard<mutex> lock(m);
            stopped = true;
        }
        cv.notify_all();
    }

    bool pop(T& item) {
        unique_lock<mutex> lock(m);

        cv.wait(lock, [&] {
            return !q.empty() || stopped;
        });

        if (q.empty()) {
            return false; // stopped and no data
        }

        item = q.front();
        q.pop();
        return true;
    }
};