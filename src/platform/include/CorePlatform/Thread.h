#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <memory>
#include <chrono>

namespace CorePlatform {

// 线程优先级枚举 (跨平台抽象)
enum class ThreadPriority {
    Idle,     // 最低优先级
    Lowest,
    BelowNormal,
    Normal,   // 默认优先级
    AboveNormal,
    Highest,
    RealTime  // 最高优先级 (使用时需要权限)
};

class Mutex final {
public:
    Mutex();
    ~Mutex();

    void Lock();
    bool TryLock();
    void Unlock();
    
    // 添加原生句柄访问器（平台无关接口）
    void* GetNativeHandle();

    // 删除拷贝构造和赋值
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class LockGuard final {
public:
    explicit LockGuard(Mutex& mutex);
    ~LockGuard();
    
    // 添加互斥量访问器
    Mutex& GetMutex() const { return mutex_; }

    // 删除拷贝构造和赋值
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    Mutex& mutex_;
};

// 条件变量 (跨平台实现)
class ConditionVariable final {
public:
    ConditionVariable();
    ~ConditionVariable();

    void NotifyOne();
    void NotifyAll();
    void Wait(LockGuard& lock);
    bool WaitFor(LockGuard& lock, std::chrono::milliseconds timeout);

    // 删除拷贝构造和赋值
    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 线程类 (核心功能)
class Thread final {
public:
    using ThreadFunc = std::function<void()>;

    // 构造函数 (不立即启动线程)
    Thread();
    
    // 析构函数 (自动销毁资源)
    ~Thread();

    // 启动线程 (传入线程函数)
    bool Start(ThreadFunc func, const std::string& threadName = "");

    // 等待线程结束
    void Join();
    
    // 分离线程 (系统自动回收)
    void Detach();
    
    // 设置线程优先级
    bool SetPriority(ThreadPriority priority);
    
    // 获取线程ID
    uint64_t GetId() const;
    
    // 设置当前线程名称
    static void SetCurrentThreadName(const std::string& name);
    
    // 获取当前线程ID
    static uint64_t GetCurrentThreadId();
    
    // 让出当前线程时间片
    static void YieldCurrentThread();
    
    // 睡眠当前线程
    static void SleepFor(std::chrono::milliseconds duration);

    // 删除拷贝构造和赋值
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CorePlatform