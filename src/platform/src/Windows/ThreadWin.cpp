// CorePlatform/src/Thread_Windows.cpp
#include "CorePlatform/Thread.h"
#include <windows.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <process.h>  // 包含 _beginthreadex
#include <cassert>
#include <system_error>
#include <memory>
#include <string>

// Windows 线程函数签名
using ThreadProc = unsigned(__stdcall*)(void*);

namespace CorePlatform {

// ==================== Mutex 实现 ====================
class Mutex::Impl {
public:
    Impl() {
        InitializeCriticalSection(&cs_);
    }
    
    ~Impl() {
        DeleteCriticalSection(&cs_);
    }
    
    void Lock() {
        EnterCriticalSection(&cs_);
    }
    
    bool TryLock() {
        return TryEnterCriticalSection(&cs_) != 0;
    }
    
    void Unlock() {
        LeaveCriticalSection(&cs_);
    }

    CRITICAL_SECTION* GetCriticalSection() {
        return &cs_;
    }

private:
    CRITICAL_SECTION cs_;
};

Mutex::Mutex() : impl_(std::make_unique<Impl>()) {}
Mutex::~Mutex() = default;
void Mutex::Lock() { impl_->Lock(); }
bool Mutex::TryLock() { return impl_->TryLock(); }
void Mutex::Unlock() { impl_->Unlock(); }

// 实现原生句柄访问器
void* Mutex::GetNativeHandle() {
    return static_cast<Impl*>(impl_.get())->GetCriticalSection();
}

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
        InitializeConditionVariable(&cv_);
    }
    
    ~Impl() = default;
    
    void NotifyOne() {
        WakeConditionVariable(&cv_);
    }
    
    void NotifyAll() {
        WakeAllConditionVariable(&cv_);
    }
    
    void Wait(LockGuard& lock) {
        auto* criticalSection = static_cast<CRITICAL_SECTION*>(lock.GetMutex().GetNativeHandle());
        SleepConditionVariableCS(&cv_, criticalSection, INFINITE);
    }
    
    bool WaitFor(LockGuard& lock, std::chrono::milliseconds timeout) {
        auto* criticalSection = static_cast<CRITICAL_SECTION*>(lock.GetMutex().GetNativeHandle());
        BOOL result = SleepConditionVariableCS(&cv_, criticalSection, static_cast<DWORD>(timeout.count()));
        return result != 0;
    }

private:
    CONDITION_VARIABLE cv_;
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
    Impl() : handle_(nullptr), threadId_(0) {}
    
    ~Impl() {
        if (handle_) {
            CloseHandle(handle_);
        }
    }
    
    bool Start(Thread::ThreadFunc func, const std::string& threadName) {
        // 创建线程启动数据
        auto startData = new ThreadStartData{std::move(func), threadName};
        
        // 创建线程
        unsigned int threadId = 0;
        handle_ = reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr,
            0,
            &ThreadProc,
            startData,
            0,
            &threadId
        ));
        
        if (!handle_) {
            delete startData;
            return false;
        }
        
        threadId_ = threadId;
        return true;
    }
    
    void Join() {
        if (handle_) {
            WaitForSingleObject(handle_, INFINITE);
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    
    void Detach() {
        if (handle_) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    
    bool SetPriority(ThreadPriority priority) {
        if (!handle_) return false;
        
        int winPriority;
        switch (priority) {
            case ThreadPriority::Idle: winPriority = THREAD_PRIORITY_IDLE; break;
            case ThreadPriority::Lowest: winPriority = THREAD_PRIORITY_LOWEST; break;
            case ThreadPriority::BelowNormal: winPriority = THREAD_PRIORITY_BELOW_NORMAL; break;
            case ThreadPriority::Normal: winPriority = THREAD_PRIORITY_NORMAL; break;
            case ThreadPriority::AboveNormal: winPriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
            case ThreadPriority::Highest: winPriority = THREAD_PRIORITY_HIGHEST; break;
            case ThreadPriority::RealTime: winPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
            default: winPriority = THREAD_PRIORITY_NORMAL;
        }
        
        return SetThreadPriority(handle_, winPriority) != 0;
    }
    
    uint64_t GetId() const {
        return static_cast<uint64_t>(threadId_);
    }

private:
    struct ThreadStartData {
        Thread::ThreadFunc func;
        std::string threadName;
    };

    static unsigned __stdcall ThreadProc(void* arg) {
        std::unique_ptr<ThreadStartData> data(static_cast<ThreadStartData*>(arg));
        
        // 设置线程名称
        if (!data->threadName.empty()) {
            Thread::SetCurrentThreadName(data->threadName);
        }
        
        // 执行线程函数
        data->func();
        return 0;
    }
    
    HANDLE handle_;
    unsigned long threadId_;
};

// ==================== 静态方法实现 ====================
void Thread::SetCurrentThreadName(const std::string& name) {
    // Windows 10+ 支持 SetThreadDescription
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32) {
        // 使用类型别名消除转换警告
        using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        
        auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
            GetProcAddress(kernel32, "SetThreadDescription"));
        
        if (setThreadDescription) {
            // 转换UTF-8到宽字符
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
            if (wideLen > 0) {
                std::wstring wideName(static_cast<size_t>(wideLen), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wideName.data(), wideLen);
                setThreadDescription(GetCurrentThread(), wideName.c_str());
            }
        }
    }
}

uint64_t Thread::GetCurrentThreadId() {
    return static_cast<uint64_t>(::GetCurrentThreadId());
}

void Thread::YieldCurrentThread() {
    SwitchToThread();
}

void Thread::SleepFor(std::chrono::milliseconds duration) {
    Sleep(static_cast<DWORD>(duration.count()));
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