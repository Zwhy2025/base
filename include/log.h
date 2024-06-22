/**
 * @file   log.h
 * @brief  一个轻量级head-only日志系统
 * @date   2024-06-22
 * @version 1.0
 * @author
 *   - zhao jun \<zwhy2025@gmail.com\>
 *
 * @note 使用方法：
 *   - 调用INIT_LOG()初始化日志系统,可传入日志等级与文件
 *   - 使用LOG_'level' << message 记录日志
 *   - level有四种：DBUG, INFO, WARN, EROR
 *   - 日志会自动记录时间，线程PID,文件名，函数名，行号等信息
 *   - 使用WIAT_LOG()等待异步日志写入完成
 */

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <queue>
#include <thread>  
#include <condition_variable>  

#ifdef __linux__
    #include <sys/syscall.h>
#endif

// 定义日志级别
enum class LogLevel {
    DBUG,     ///< 调试
    INFO,     ///< 信息
    WARN,     ///< 警告
    EROR,     ///< 错误
};

// 定义控制台颜色代码
enum class ConsoleColor {
    DEFAULT = 0,
    BLACK = 30,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE
};

/**
 * @brief 日志系统异常类，继承自 std::exception
 */
class LogException : public std::exception {
private:
    std::string message;  ///< 异常消息

public:
    /**
     * @brief 构造一个 LogException 实例
     * @param msg 异常消息
     */
    LogException(const std::string& msg) : message(msg) {}
    
    /**
     * @brief 获取异常消息
     * @return const char* 异常消息
     */
    const char* what() const noexcept override {
        return message.c_str();
    }
};

/**
 * @brief RAII机制类，用于设置和重置控制台输出颜色
 */
class ConsoleColorSetter {
public:
    /**
     * @brief 构造 ConsoleColorSetter 实例，设置控制台输出颜色
     * @param color 控制台颜色
     */
    ConsoleColorSetter(ConsoleColor color) {
        std::ostringstream oss;
        oss << "\033[" << static_cast<int>(color) << "m";
        _strColorCode = oss.str();
    }

    /**
     * @brief 析构 ConsoleColorSetter 实例，重置控制台输出颜色
     */
    ~ConsoleColorSetter() {
        _resetColor();
    }

    /**
     * @brief 输出控制台颜色码到流
     * @param os 输出流
     * @param colorSetter ConsoleColorSetter 实例
     * @return std::ostream& 输出流
     */
    friend std::ostream& operator<<(std::ostream& os, const ConsoleColorSetter& colorSetter) {
        os << colorSetter._strColorCode;
        return os;
    }

private:
    /**
     * @brief 重置控制台输出颜色
     */
    void _resetColor() {
        std::cout << "\033[0m"; // 重置颜色
    }

private:
    std::string _strColorCode;  ///< 控制台颜色码
};

/**
 * @brief 日志系统类，实现单例模式
 */
class Logger {
public:
    /**
     * @brief 获取 Logger 的单例实例
     * @return Logger& Logger 实例的引用
     */
    static Logger& getInstance() {
        static Logger _tInstance;
        return _tInstance;
    }

    /**
     * @brief 初始化日志系统，设置日志等级和日志文件，并启动异步处理线程
     * @param level 日志等级
     * @param filename 日志文件名
     */
    void init(LogLevel level, const std::string& filename) {
        _setLogLevel(level);
        _setLogFile(filename);

        // 启动后台处理线程
        _bStopThread = false;
        _tAsyncThread = std::thread(&Logger::_processLogQueue, this);
    }

    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param message 日志消息
     * @param file 文件名
     * @param func 函数名
     * @param line 行号
     */
    void log(LogLevel level, const std::string& message, const char* file, const char* func, int line) {
        
        if (_bStopThread)
        {
            // 抛出异常未初始化
            throw LogException("Logger has not been initialized");
        }
        
        if (level >= _eLogLevel) {
            std::lock_guard<std::mutex> guard(_tMutex);
            _LogQueue.push(std::make_tuple(level, message, file, func, line));
            _tLogQueueCondition.notify_one(); // 唤醒后台线程
        }
    }

    /**
     * @brief 等待后台日志写入线程完成
     */
    void wait() {
        std::unique_lock<std::mutex> lock(_tMutex);
        _tLogQueueCondition.wait_for(lock, std::chrono::milliseconds(1), [this]() {
             return _LogQueue.empty(); 
        });
    }

    /**
     * @brief 停止日志系统，等待后台线程结束
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> guard(_tMutex);
            _bStopThread = true;
        }
        _tLogQueueCondition.notify_one();   // 唤醒后台线程
        _tAsyncThread.join();               // 等待后台线程结束
    }

private:
    /**
     * @brief Logger 类的构造函数
     */
    Logger() : _eLogLevel(LogLevel::INFO), _eConsoleColorThreshold(LogLevel::WARN) {}

    /**
     * @brief Logger 类的析构函数
     */
    ~Logger() {
        if (_tFileStream.is_open()) {
            _tFileStream.close();
        }
    }

    /**
     * @brief 禁止 Logger 类的拷贝构造函数
     */
    Logger(const Logger&) = delete;

    /**
     * @brief 禁止 Logger 类的赋值操作符重载
     */
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 将日志级别转换为对应字符串
     * @param level 日志级别
     * @return std::string 日志级别对应的字符串
     */
    std::string _logLevelToString(LogLevel level) {
        switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::EROR: return "EROR";
        case LogLevel::DBUG: return "DBUG";
        default: return "UNKNOWN";
        }
    }

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void _setLogLevel(LogLevel level) {
        _eLogLevel = level;
    }

    /**
     * @brief 设置日志文件
     * @param filename 日志文件名
     */
    void _setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> guard(_tMutex); // 线程安全

        if (_tFileStream.is_open()) {
            _tFileStream.close();
        }
        _tFileStream.open(filename, std::ios::out | std::ios::app);
        if (!_tFileStream) {
            //std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }

    /**
     * @brief 生成格式化的日志信息字符串
     * @param level 日志级别
     * @param message 日志消息
     * @param file 文件名
     * @param func 函数名
     * @param line 行号
     * @return std::string 格式化后的日志信息字符串
     */
    std::string _formatLogMessage(LogLevel level, const std::string& message, const char* file, const char* func, int line) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto s = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        std::tm* tm = std::localtime(&s);

        std::ostringstream oss;
        oss << "[" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        oss << "[" << _logLevelToString(level) << "]";
        oss << "[" << _getProcessId()<< "]: " << message;
        oss << " (" << _extractFilename(file) << " " << func << ":" << line << ")" << std::endl;

        return oss.str();
    }

    /**
     * @brief 获取进程ID
     * @return unsigned long 进程ID
     */
    unsigned long _getProcessId() {
        #ifdef __linux__
            return static_cast<unsigned long>(syscall(SYS_gettid));
        #elif _WIN32
            return GetCurrentProcessId();
        #endif
    }

    /**
     * @brief 从文件路径中提取文件名
     * @param filePath 文件路径
     * @return std::string 文件名
     */
    std::string _extractFilename(const char* filePath) {
        const char* filename = filePath;
        const char* ptr = filePath;

        while (*ptr) {
            if (*ptr == '/' || *ptr == '\\') {
                filename = ptr + 1; // 文件名是路径中最后一个'/'或'\'之后的字符串
            }
            ptr++;
        }

        return filename;
    }

    /**
     * @brief 后台线程处理日志队列
     */
    void _processLogQueue() {
        while (!_bStopThread) {
            std::unique_lock<std::mutex> lock(_tMutex);
            _tLogQueueCondition.wait(lock, [this]() { return _bStopThread || !_LogQueue.empty(); });

            while (!_LogQueue.empty()) {
                auto logEntry = _LogQueue.front();
                _LogQueue.pop();
                lock.unlock(); // 在写入日志时释放锁

                std::string logMessage = _formatLogMessage(std::get<0>(logEntry), std::get<1>(logEntry), std::get<2>(logEntry), std::get<3>(logEntry), std::get<4>(logEntry));

                if (_tFileStream.is_open()) {
                    _tFileStream << logMessage;
                    _tFileStream.flush(); // 立即刷新文件流
                }

                // 输出到控制台
                std::ostringstream coloredMessage;
                switch (std::get<0>(logEntry)) {
                    case LogLevel::DBUG:
                        coloredMessage << ConsoleColorSetter(ConsoleColor::GREEN);
                        break;
                    case LogLevel::INFO:
                        coloredMessage << ConsoleColorSetter(ConsoleColor::DEFAULT);
                        break;
                    case LogLevel::WARN:
                        coloredMessage << ConsoleColorSetter(ConsoleColor::YELLOW);
                        break;
                    case LogLevel::EROR:
                        coloredMessage << ConsoleColorSetter(ConsoleColor::RED);
                        break;
                    default:
                        break;
                }
                coloredMessage << logMessage;
                coloredMessage << ConsoleColorSetter(ConsoleColor::DEFAULT);
                std::cout << coloredMessage.str();

                lock.lock(); // 重新获取锁
            }
        }
    }

 private:
    bool _bStopThread = true;                    ///< 标记是否停止后台线程
    std::mutex _tMutex;                          ///< 互斥量，保证线程安全
    LogLevel _eLogLevel;                         ///< 日志级别
    std::thread _tAsyncThread;                   ///< 后台处理线程
    std::ofstream _tFileStream;                  ///< 文件流
    LogLevel _eConsoleColorThreshold;            ///< 控制台输出颜色的阈值
    std::condition_variable _tLogQueueCondition; ///< 条件变量，用于通知日志队列的状态变化
    std::queue<std::tuple<LogLevel, std::string, const char*, const char*, int>> _LogQueue; ///< 日志消息队列
};

/**
 * @brief 日志流操作类，用于构建和记录日志消息
 */
class LogStream {
public:
    /**
     * @brief LogStream 构造函数，初始化日志流操作类
     * @param level 日志级别
     * @param file 文件名
     * @param func 函数名
     * @param line 行号
     */
    LogStream(LogLevel level, const char* file, const char* func, int line)
        : _tLevel(level), _strFilePath(file), _strFuncName(func), _nLine(line) {}

    /**
     * @brief LogStream 析构函数，记录日志消息到日志系统
     */
    ~LogStream() {
        Logger::getInstance().log(_tLevel, _tStream.str(), _strFilePath, _strFuncName, _nLine);
    }

    /**
     * @brief 模板重载操作符<<，用于向日志流中添加数据
     * @tparam T 数据类型
     * @param value 要添加的数据
     * @return LogStream& 日志流操作类的引用
     */
    template<typename T>
    LogStream& operator<<(const T& value) {
        _tStream << value;
        return *this;
    }

private:
    LogLevel _tLevel;            ///< 日志级别
    const char* _strFilePath;    ///< 文件名
    const char* _strFuncName;    ///< 函数名
    int _nLine;                  ///< 行号
    std::ostringstream _tStream; ///< 日志流
};

/**
 * @brief 日志系统初始化类，用于初始化日志系统并管理其生命周期
 */
class LogInit {
public:
    /**
     * @brief 获取 LogInit 的单例实例，用于初始化日志系统
     * @param level 日志等级，默认为 DBUG
     * @param filename 日志文件名，默认为空字符串
     * @return LogInit& LogInit 实例的引用
     */
    static LogInit& getInstance(LogLevel level = LogLevel::DBUG, const std::string& filename = "") {
        static LogInit instance(level, filename); // 使用静态局部变量确保单例
        return instance;
    }

    /**
     * @brief 禁止 LogInit 类的拷贝构造函数
     */
    LogInit(const LogInit&) = delete;

    /**
     * @brief 禁止 LogInit 类的赋值操作符重载
     */
    LogInit& operator=(const LogInit&) = delete;

private:
    /**
     * @brief LogInit 构造函数，初始化日志系统
     * @param level 日志等级
     * @param filename 日志文件名
     */
    LogInit(LogLevel level, const std::string& filename) {
        Logger::getInstance().init(level, filename);
    }

    /**
     * @brief LogInit 析构函数，关闭日志系统
     */
    ~LogInit() {
        Logger::getInstance().shutdown();
    }
};

/**
 * @brief 初始化日志宏，用于简化日志系统的初始化操作
 */
#define INIT_LOG(...) LogInit::getInstance(__VA_ARGS__)

/**
 * @brief 等待异步日志写入完成的宏
 */
#define WIAT_LOG() Logger::getInstance().wait()

/**
 * @brief 日志记录宏，用于方便记录不同级别的日志消息
 */
#define LOG(level) LogStream(level, __FILE__, __func__, __LINE__)
#define LOG_DBUG LOG(LogLevel::DBUG)
#define LOG_INFO LOG(LogLevel::INFO)
#define LOG_WARN LOG(LogLevel::WARN)
#define LOG_EROR LOG(LogLevel::EROR)
