#include "toml/toml.hpp"
#include <iostream>
#include <string>
#include <iostream>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
// #include "spdlog/sinks/rotating_compress_file_sink.h"

#include <csignal>
#include <atomic>
#include <thread>

// 全局运行标志，信号处理函数里置 false
std::atomic<bool> g_running{true};

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

struct DmcConfig {
    std::string log_file_full_name = "./logs/app.log";
    int log_file_cycles = 10;
    int log_file_maxsize = 10485760;
    spdlog::level::level_enum log_level = spdlog::level::info;
    // int log_level = 1; // 0: debug, 1: info, 2: warn, 3: error
    std::string log_type = "all";
};

class AppConfig
{
  public:
    // 保留完整的 TOML 文档，不对字段名、层级或字段类型作预设。
    const toml::value& document() const
    {
        return document_;
    }

    // toml::value& document()
    // {
    //     return document_;
    // }

    bool contains(const std::string& key) const
    {
        return document_.is_table() && document_.contains(key);
    }

    // 支持编译期已知的任意层级字段，例如 get<std::string>("server", "ip")。
    template <typename T, typename... Keys>
    T get(const Keys&... keys) const
    {
        return toml::find<T>(document_, keys...);
    }

    // 支持运行时才知道的字段路径，例如 {"server", "ip"}。
    const toml::value* find_value(const std::vector<std::string>& path) const
    {
        const toml::value* current = &document_;
        for (std::vector<std::string>::const_iterator it = path.begin();
             it != path.end(); ++it)
        {
            if (!current->is_table() || !current->contains(*it))
            {
                return 0;
            }
            current = &current->at(*it);
        }
        return current;
    }

    template <typename T>
    T get(const std::vector<std::string>& path) const
    {
        const toml::value* value = find_value(path);
        if (value == 0)
        {
            throw std::out_of_range("配置字段不存在");
        }
        return toml::get<T>(*value);
    }

bool load_from_file(const char* persistPath)
{
    // Call external function for file system validation
    if (!CheckConfigFile(persistPath))
    {
        std::cout << "[ERROR] Config file validation failed: " << persistPath << std::endl;
        return false;
    }

    try
    {
        std::ifstream fs(persistPath);
        if (!fs.is_open())
        {
            std::cout << "[ERROR] Failed to open config file: " << persistPath << std::endl;
            return false;
        }
        // Read and parse at one‑shot, including TOML syntax validation
        document_ = toml::parse(fs, persistPath);
    }
    catch (const toml::syntax_error& e)
    {
        std::cout << "[ERROR] TOML syntax error: " << e.what() << std::endl;
        return false;
    }
    catch (const toml::file_io_error& e)
    {
        std::cout << "[ERROR] Config file I/O error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e)
    {
        std::cout << "[ERROR] Exception while loading config: " << e.what() << std::endl;
        return false;
    }

    return true;
}
    void Config_Logsetting(const AppConfig& app_cfg, DmcConfig &cfg){
    const toml::value& data = app_cfg.document();
    // ---- dmc ----
    if (data.contains("dmc")) {
        const auto& t = data.at("dmc");
        cfg.log_file_full_name = toml::find_or<std::string>(t, "log_file_full_name", "./logs/app.log");
        cfg.log_file_cycles = toml::find_or<int>(t, "log_file_cycles", 10);
        cfg.log_file_maxsize = toml::find_or<int>(t, "log_file_maxsize", 10485760);
        cfg.log_type = toml::find_or<std::string>(t, "log_type", "all");
        const std::string Slog_level = toml::find_or<std::string>(t, "log_level", "info");
        if (Slog_level == "debug") {
            cfg.log_level = spdlog::level::debug;
        } else if (Slog_level == "info") {
            cfg.log_level = spdlog::level::info;
        } else if (Slog_level == "warn") {
            cfg.log_level = spdlog::level::warn;
        } else if (Slog_level == "error") {
            cfg.log_level = spdlog::level::err;
        } else {
            cfg.log_level = spdlog::level::info;
        }
    }
}
  private:
    bool CheckConfigFile(const char* const persistPath)
{
    if (persistPath == nullptr)
    {
        std::cout << "[ERROR] Config path is nullptr" << std::endl;
        return false;
    }

    size_t len = std::strlen(persistPath);
    if (len == 0U)
    {
        std::cout << "[ERROR] Config path is empty string" << std::endl;
        return false;
    }

    struct stat st{};
    if (stat(persistPath, &st) != 0)
    {
        std::cout << "[ERROR] stat failed, file may not exist: " << persistPath << std::endl;
        return false;
    }

    if (!S_ISREG(st.st_mode))
    {
        std::cout << "[ERROR] Not a regular file: " << persistPath << std::endl;
        return false;
    }

    if (access(persistPath, R_OK) != 0)
    {
        std::cout << "[ERROR] File is not readable: " << persistPath << std::endl;
        return false;
    }
    return true;
}
  private:
    toml::value document_;
};

bool init_log(const DmcConfig& config)
{
    try
    {
        std::vector<spdlog::sink_ptr> sinks;
        if(config.log_type == "console" || config.log_type == "all"){
            // 创建控制台输出槽
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(config.log_level);
            console_sink->set_pattern("[%H:%M:%S] [%^%l%$] [%@] [process %P:thread %t] %v");
            sinks.push_back(console_sink);
        }
        if(config.log_type == "file" || config.log_type == "all"){
            // 创建文件输出槽
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file_full_name,
                config.log_file_maxsize,
                config.log_file_cycles,false);
            file_sink->set_level(config.log_level);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%@] [process %P:thread %t] %v");
            sinks.push_back(file_sink);
        }
        // 创建多重输出槽的logger
        auto logger = std::make_shared<spdlog::logger>("AppLogger", sinks.begin(), sinks.end());
        logger->set_level(config.log_level);

        // 设置为默认logger
        spdlog::set_default_logger(logger);

        // 设置自动刷新
        spdlog::flush_every(std::chrono::seconds(1));
    }
    catch (const spdlog::spdlog_ex& exception)
    {
        std::cout << "[ERROR] Log initialization failed: " << exception.what() << std::endl;
        return false;
    }
    catch (const std::exception& exception)
    {
        std::cout << "[ERROR] An error occurred during log initialization: " << exception.what() << std::endl;
        return false;
    }

    return true;
}

int main()
{
    // 注册 Ctrl+C (SIGINT) 和终止信号
    // std::signal(SIGINT, signal_handler);
    // std::signal(SIGTERM, signal_handler);
    int ret = 0;
    const char* persistPath = "./config/config_or.default";
    AppConfig app_cfg;
    // =========创建对象后，只读取一次文件到内存=========
    app_cfg.load_from_file(persistPath);
    DmcConfig dmc_cfg;
    //后续解析直接复用内存里的app_cfg对象，不再读磁盘
    app_cfg.Config_Logsetting(app_cfg, dmc_cfg);
    if (!init_log(dmc_cfg))
    {
    std::cout << "[ERROR] Log initialization failed" << std::endl;
        return -1;
    }
    std::cout << dmc_cfg.log_file_full_name << std::endl;
    std::cout << dmc_cfg.log_file_cycles << std::endl;
    std::cout << dmc_cfg.log_file_maxsize << std::endl;
    std::cout << dmc_cfg.log_level << std::endl;
    // while(g_running)
    // {

    SPDLOG_INFO("retnum: {}", ret);
    SPDLOG_INFO("retnum: {}", ret+1);
    SPDLOG_INFO("retnum: {}", ret+2);
    SPDLOG_INFO("retnum: {}", ret+3);
    SPDLOG_INFO("retnum: {}", ret+4);
    // ret = ret + 5;
    // switch (dmc_cfg.log_level) {
    //     case spdlog::level::debug:
    //         SPDLOG_INFO("Log level: debug");
    //         break;
    //     case spdlog::level::info:
    //         SPDLOG_INFO("Log level: info");
    //         break;
    //     case spdlog::level::warn:
    //         SPDLOG_INFO("Log level: warn");
    //         break;
    //     case spdlog::level::err:
    //         SPDLOG_INFO("Log level: error");
    //         break;
    //     default:
    //         SPDLOG_INFO("Log level: unknown");
    //         break;
    // }

    // /////test:warn
    // SPDLOG_WARN("Log initialized successfully");
    // SPDLOG_WARN("Log file: {}", dmc_cfg.log_file_full_name);
    // SPDLOG_WARN("Log cycles: {}", dmc_cfg.log_file_cycles);
    // SPDLOG_WARN("Log max size: {}", dmc_cfg.log_file_maxsize);

    // /////test:error
    // SPDLOG_ERROR("Log initialized successfully");
    // SPDLOG_ERROR("Log file: {}", dmc_cfg.log_file_full_name);
    // SPDLOG_ERROR("Log cycles: {}", dmc_cfg.log_file_cycles);
    // SPDLOG_ERROR("Log max size: {}", dmc_cfg.log_file_maxsize);

    // 加一点延时，避免疯狂写日志导致 I/O 阻塞、CPU 占满
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));
// }
    // dmc_cfg.test = toml::find_or<std::string>(app_cfg.document(),"app", "log_file_full_name", "/tmp/logs/app.log");
    
    // 优雅退出：刷新所有缓冲区、关闭文件句柄
    // spdlog::shutdown();
    return 0;
}