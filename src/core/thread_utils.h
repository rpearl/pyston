// Copyright (c) 2014 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PYSTON_CORE_THREADUTILS_H
#define PYSTON_CORE_THREADUTILS_H

#include <functional>
#include <pthread.h>
#include <unordered_map>

#include "core/common.h"

namespace pyston {
namespace threading {


class LockBase {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    virtual ~LockBase() {}
};

class _LockedRegion {
private:
    LockBase* const mutex;

public:
    _LockedRegion(LockBase* m) : mutex(m) { mutex->lock(); }
    _LockedRegion(LockBase& m) : mutex(&m) { mutex->lock(); }
    ~_LockedRegion() { mutex->unlock(); }
};

//_LockedRegion _makeLockedRegion(LockBase* mutex) {
//    return _LockedRegion(mutex);
//}
//_LockedRegion _makeLockedRegion(LockBase& mutex) {
//    return _LockedRegion(&mutex);
//}
//
#define LOCK_REGION(lock) auto CAT(_lock_, __LINE__) = pyston::threading::_LockedRegion(lock)

class NopLock : public LockBase {
public:
    void lock() {}
    void unlock() {}

    NopLock* asRead() { return this; }
    NopLock* asWrite() { return this; }
};

class PthreadCond;

class PthreadMutexBase : public LockBase {
    friend class PthreadCond;

protected:
    pthread_mutex_t mutex;

public:
    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }

    PthreadMutexBase* asRead() { return this; }
    PthreadMutexBase* asWrite() { return this; }
};

class PthreadFastMutex : public PthreadMutexBase {
public:
    PthreadFastMutex() { mutex = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP; }
};

class PthreadMutex : public PthreadMutexBase {
public:
    PthreadMutex() { mutex = PTHREAD_MUTEX_INITIALIZER; }
};

class PthreadCond {
private:
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

public:
    int wait(PthreadMutexBase* m) { return pthread_cond_wait(&cond, &m->mutex); }
    int notify() { return pthread_cond_signal(&cond); }
    int notifyAll() { return pthread_cond_broadcast(&cond); }
};

class PthreadRWLock {
private:
    pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

public:
    class PthreadRWLockRead {
    private:
        pthread_rwlock_t rwlock;
        PthreadRWLockRead() = delete;

    public:
        void lock() { pthread_rwlock_rdlock(&rwlock); }
        void unlock() { pthread_rwlock_unlock(&rwlock); }
    };

    class PthreadRWLockWrite {
    private:
        pthread_rwlock_t rwlock;
        PthreadRWLockWrite() = delete;

    public:
        void lock() { pthread_rwlock_wrlock(&rwlock); }
        void unlock() { pthread_rwlock_unlock(&rwlock); }
    };

    PthreadRWLockRead* asRead() { return reinterpret_cast<PthreadRWLockRead*>(this); }

    PthreadRWLockWrite* asWrite() { return reinterpret_cast<PthreadRWLockWrite*>(this); }
};

class PthreadSpinLock {
private:
    pthread_spinlock_t spinlock;

public:
    PthreadSpinLock() { pthread_spin_init(&spinlock, false); }

    void lock() { pthread_spin_lock(&spinlock); }
    void unlock() { pthread_spin_unlock(&spinlock); }

    PthreadSpinLock* asRead() { return this; }
    PthreadSpinLock* asWrite() { return this; }
};


namespace impl {
// From http://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
template <int...> struct seq {};

template <int N, int... S> struct gens : gens<N - 1, N - 1, S...> {};

template <int... S> struct gens<0, S...> { typedef seq<S...> type; };
}

template <typename T, typename... CtorArgs> class PerThreadSet {
private:
    pthread_key_t pthread_key;
    PthreadFastMutex lock;

    struct Storage {
        PerThreadSet<T, CtorArgs...>* self;
        T val;
    };

    std::unordered_map<pthread_t, Storage*> map;
    std::tuple<CtorArgs...> ctor_args;

    static void dtor(void* val) {
        Storage* s = static_cast<Storage*>(val);
        assert(s);

        auto* self = s->self;
        LOCK_REGION(&self->lock);

        // I assume this destructor gets called on the same thread
        // that this data is bound to:
        assert(self->map.count(pthread_self()));
        self->map.erase(pthread_self());

        delete s;
    }

    template <int... S> Storage* make(impl::seq<S...>) {
        return new Storage{ .self = this, .val = T(std::get<S>(ctor_args)...) };
    }

public:
    PerThreadSet(CtorArgs... ctor_args) : ctor_args(std::forward<CtorArgs>(ctor_args)...) {
        int code = pthread_key_create(&pthread_key, &dtor);
    }

    void forEachValue(std::function<void(T*)> f) {
        LOCK_REGION(&lock);

        for (auto& p : map) {
            f(&p.second->val);
        }
    }

    template <typename... Arguments> void forEachValue(std::function<void(T*, Arguments...)> f, Arguments... args) {
        LOCK_REGION(&lock);

        for (auto& p : map) {
            f(&p.second->val, std::forward<Arguments>(args)...);
        }
    }

    T* get() {
        // Is there even much benefit to using pthread_getspecific here, as opposed to looking
        // it up in the map?  I suppose it avoids locking
        Storage* s = static_cast<Storage*>(pthread_getspecific(pthread_key));
        if (!s) {
            s = make(typename impl::gens<sizeof...(CtorArgs)>::type());

            LOCK_REGION(&lock);
            int code = pthread_setspecific(pthread_key, s);
            assert(code == 0);

            map[pthread_self()] = s;
        }
        return &s->val;
    }
};

} // namespace threading
} // namespace pyston

#endif
