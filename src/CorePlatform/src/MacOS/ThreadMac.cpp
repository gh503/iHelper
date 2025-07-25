// CorePlatform/src/Thread_MacOS.cpp
#include "CorePlatform/Thread.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <system_error>
#include <memory>

namespace CorePlatform {

// ==================== Mutex 实现 ====================
class Mutex::Impl {
public:
    Impl() {
        pthread_mutexattr_init(&attr_);
        pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&mutex_, &attr_);
    }
    
    ~Impl() {
        pthread_mutex_destroy(&mutex_);
        pthread_mutexattr_destroy(&attr_);
    }
    
    void Lock() {
        pthread_mutex_lock(&mutex_);
    }
    
    bool TryLock() {
        return pthread_mutex_trylock(&mutex_) == 0;
    }
    
    void Unlock() {
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
    pthread_mutexattr_t attr_;
};

Mutex::Mutex() : impl_(std::make_unique<Impl>()) {}
Mutex::~Mutex() = default;
void Mutex::Lock() { impl_->Lock(); }
bool Mutex::TryLock() { return impl_->TryLock(); }
void Mutex::Unlock() { impl_->Unlock(); }

// ==================== LockGuard 实现 ====================
LockGuard::LockGuard(Mutex& mutex) : mutex_(mutex) { 
    mutex_.Lock(); 
}

LockGuard::~LockGuard() { 
    mutex_.Unlock(); 
}

// ==================== ConditionVariable 实现 ====================
class ConditionVariable::Impl {
public:
    Impl() {
        pthread_cond_init(&cond_, nullptr);
    }
    
    ~Impl() {
        pthread_cond_destroy(&cond_);
    }
    
    void NotifyOne() {
        pthread_cond_signal(&cond_);
    }
    
    void NotifyAll() {
        pthread_cond_broadcast(&cond_);
    }
    
    void Wait(LockGuard& lock) {
        pthread_cond_wait(&cond_, &static_cast<Mutex::Impl*>(lock.mutex_.impl_.get())->mutex_);
    }
    
    bool WaitFor(LockGuard& lock, std::chrono::milliseconds timeout) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        
        // 计算超时时间
        auto nsec = timeout.count() * 1000000LL;
        ts.tv_sec += nsec / 1000000000LL;
        ts.tv_nsec += nsec % 1000000000LL;
        if (ts.tv_nsec >= 1000000000LL) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000LL;
        }
        
        return pthread_cond_timedwait(&cond_, 
            &static_cast<Mutex::Impl*>(lock.mutex_.impl_.get())->mutex_, 
            &ts) == 0;
    }

private:
    pthread_cond_t cond_;
};

ConditionVariable::ConditionVariable() : impl_(std::make_unique<Impl>()) {}
ConditionVariable::~ConditionVariable() = default;
void ConditionVariable::NotifyOne() { impl_->NotifyOne(); }
void ConditionVariable::NotifyAll() { impl_->NotifyAll(); }
void ConditionVariable::Wait(LockGuard& lock) { impl_->Wait(lock); }
bool ConditionVariable::WaitFor(LockGuard& lock, std::chrono::milliseconds timeout) {
    return impl_->WaitFor(lock, timeout);
}

// ==================== Thread 实现 ====================
class Thread::Impl {
public:
    Impl() : thread_(0), started_(false) {}
    
    ~Impl() {
        if (started_ && !detached_) {
            pthread_detach(thread_);
        }
    }
    
    bool Start(Thread::ThreadFunc func, const std::string& threadName) {
        func_ = std::move(func);
        threadName_ = threadName;
        
        int result = pthread_create(&thread_, nullptr, &ThreadProc, this);
        if (result != 0) {
            return false;
        }
        
        started_ = true;
        return true;
    }
    
    void Join() {
        if (started_ && !detached_) {
            pthread_join(thread_, nullptr);
            detached_ = true;
        }
    }
    
    void Detach() {
        if (started_ && !detached_) {
            pthread_detach(thread_);
            detached_ = true;
        }
    }
    
    bool SetPriority(ThreadPriority priority) {
        if (!started_) return false;
        
        // macOS 线程优先级设置
        thread_extended_policy_data_t extendedPolicy;
        thread_precedence_policy_data_t precedencePolicy;
        
        // 设置优先级
        int relativePriority;
        switch (priority) {
            case ThreadPriority::Idle: relativePriority = -15; break;
            case ThreadPriority::Lowest: relativePriority = -10; break;
            case ThreadPriority::BelowNormal: relativePriority = -5; break;
            case ThreadPriority::Normal: relativePriority = 0; break;
            case ThreadPriority::AboveNormal: relativePriority = 5; break;
            case ThreadPriority::Highest: relativePriority = 10; break;
            case ThreadPriority::RealTime: relativePriority = 15; break;
            default: relativePriority = 0;
        }
        
        // 获取mach端口
        mach_port_t machThread = pthread_mach_thread_np(thread_);
        
        // 设置扩展策略
        extendedPolicy.timeshare = (priority == ThreadPriority::RealTime) ? 0 : 1;
        thread_policy_set(machThread, THREAD_EXTENDED_POLICY,
                          (thread_policy_t)&extendedPolicy,
                          THREAD_EXTENDED_POLICY_COUNT);
        
        // 设置优先级
        precedencePolicy.importance = relativePriority;
        thread_policy_set(machThread, THREAD_PRECEDENCE_POLICY,
                          (thread_policy_t)&precedencePolicy,
                          THREAD_PRECEDENCE_POLICY_COUNT);
        
        return true;
    }
    
    uint64_t GetId() const {
        return static_cast<uint64_t>(pthread_mach_thread_np(thread_));
    }

private:
    static void* ThreadProc(void* arg) {
        Impl* self = static_cast<Impl*>(arg);
        
        // 设置线程名称
        if (!self->threadName_.empty()) {
            pthread_setname_np(self->threadName_.c_str());
        }
        
        // 执行线程函数
        self->func_();
        return nullptr;
    }
    
    pthread_t thread_;
    Thread::ThreadFunc func_;
    std::string threadName_;
    bool started_ = false;
    bool detached_ = false;
};

// ==================== 静态方法实现 ====================
void Thread::SetCurrentThreadName(const std::string& name) {
    pthread_setname_np(name.c_str());
}

uint64_t Thread::GetCurrentThreadId() {
    return static_cast<uint64_t>(pthread_mach_thread_np(pthread_self()));
}

void Thread::YieldCurrentThread() {
    sched_yield();
}

void Thread::SleepFor(std::chrono::milliseconds duration) {
    usleep(static_cast<useconds_t>(duration.count() * 1000));
}

// ==================== Thread 公共方法 ====================
Thread::Thread() : impl_(std::make_unique<Impl>()) {}
Thread::~Thread() = default;

bool Thread::Start(ThreadFunc func, const std::string& threadName) {
    return impl_->Start(std::move(func), threadName);
}

void Thread::Join() { impl_->Join(); }
void Thread::Detach() { impl_->Detach(); }
bool Thread::SetPriority(ThreadPriority priority) { return impl_->SetPriority(priority); }
uint64_t Thread::GetId() const { return impl_->GetId(); }

} // namespace CorePlatform