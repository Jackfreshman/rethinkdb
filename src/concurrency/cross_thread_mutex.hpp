// Copyright 2010-2014 RethinkDB, all rights reserved.
#ifndef CONCURRENCY_CROSS_THREAD_MUTEX_HPP_
#define CONCURRENCY_CROSS_THREAD_MUTEX_HPP_

#include <deque>
#include <utility>

#include "arch/spinlock.hpp"
#include "utils.hpp"

class coro_t;
class cross_thread_mutex_t;

/* `cross_thread_mutex_t` behaves like `mutex_t`, but can be used across
 * multiple threads.
 * It also allows recursive lock acquisition from the same coroutine if desired.
 * It is more efficient than doing
 * `{ on_thread_t t(mutex_home_thread); co_lock_mutex(&m); }`
 * because the thread switch is avoided.
 * In single-threaded use cases, it comes with more overhead than a regular
 * `mutex_t` though. */
class cross_thread_mutex_t {
public:
    class acq_t {
    public:
        acq_t() : lock_(NULL) { }
        explicit acq_t(cross_thread_mutex_t *l);
        ~acq_t();
        void reset();
        void reset(cross_thread_mutex_t *l);
        void assert_is_holding(DEBUG_VAR cross_thread_mutex_t *m) const {
            rassert(lock_ == m);
        }
    private:
        friend void swap(acq_t &, acq_t &);
        cross_thread_mutex_t *lock_;
        DISABLE_COPYING(acq_t);
    };

    cross_thread_mutex_t(bool recursive = false) :
        is_recursive(recursive), locked_count(0), lock_holder(NULL) { }
    ~cross_thread_mutex_t() {
#ifndef NDEBUG
        spinlock_acq_t acq(&spinlock);
        rassert(locked_count == 0);
#endif
    }

    bool is_locked() {
        spinlock_acq_t acq(&spinlock);
        return locked_count > 0;
    }

    friend void co_lock_mutex(cross_thread_mutex_t *mutex);
    friend void unlock_mutex(cross_thread_mutex_t *mutex);

private:
    spinlock_t spinlock;
    bool is_recursive;
    int locked_count;
    coro_t *lock_holder;
    std::deque<coro_t *> waiters;

    DISABLE_COPYING(cross_thread_mutex_t);
};

inline void swap(cross_thread_mutex_t::acq_t &a, cross_thread_mutex_t::acq_t &b) {
    std::swap(a.lock_, b.lock_);
}

#endif /* CONCURRENCY_CROSS_THREAD_MUTEX_HPP_ */
