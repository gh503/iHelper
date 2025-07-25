#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <utility>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API Process {
public:
    Process();
    ~Process();
    
    // 禁用拷贝
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    
    /**
     * @brief 启动一个进程
     * @param executable 可执行文件路径
     * @param arguments 命令行参数
     * @param workingDirectory 工作目录（可选）
     * @param environment 环境变量（可选）
     * @return 是否成功启动
     */
    bool Start(const std::string& executable, 
              const std::vector<std::string>& arguments = {},
              const std::string& workingDirectory = "",
              const std::vector<std::pair<std::string, std::string>>& environment = {});
              
    /**
     * @brief 检查进程是否正在运行
     */
    bool IsRunning() const;
    
    /**
     * @brief 等待进程结束
     * @param timeoutMs 超时时间（毫秒），-1表示无限等待
     * @return 是否在超时前结束
     */
    bool WaitForFinished(int timeoutMs = -1);
    
    /**
     * @brief 终止进程
     * @param force 是否强制终止
     */
    void Terminate(bool force = false);
    
    /**
     * @brief 向进程标准输入写入数据
     * @param data 要写入的数据
     * @return 是否成功写入
     */
    bool WriteInput(const std::string& data);
    
    /**
     * @brief 读取进程的标准输出
     * @param timeoutMs 读取超时时间（毫秒）
     * @return 读取到的内容
     */
    std::string ReadOutput(int timeoutMs = 0);
    
    /**
     * @brief 读取进程的标准错误
     * @param timeoutMs 读取超时时间（毫秒）
     * @return 读取到的内容
     */
    std::string ReadError(int timeoutMs = 0);
    
    /**
     * @brief 获取进程退出码
     * @return 退出码，如果进程仍在运行则返回nullopt
     */
    std::optional<int> ExitCode() const;
    
    /**
     * @brief 获取进程ID
     */
    int64_t ProcessId() const;
    
    /**
     * @brief 执行一个进程并等待其结束
     * @param executable 可执行文件路径
     * @param arguments 命令行参数
     * @param workingDirectory 工作目录（可选）
     * @param input 标准输入内容（可选）
     * @param environment 环境变量（可选）
     * @return 包含执行结果的ProcessResult对象
     */
    struct Result {
        int exitCode;
        std::string output;
        std::string error;
    };
    
    static Result Execute(const std::string& executable, 
                        const std::vector<std::string>& arguments = {},
                        const std::string& workingDirectory = "",
                        const std::string& input = "",
                        const std::vector<std::pair<std::string, std::string>>& environment = {});
                               
    /**
     * @brief 获取当前进程ID
     */
    static int64_t CurrentProcessId();
    
    /**
     * @brief 获取父进程ID
     */
    static int64_t ParentProcessId();
    
    /**
     * @brief 获取当前进程路径
     */
    static std::string CurrentProcessPath();
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace CorePlatform