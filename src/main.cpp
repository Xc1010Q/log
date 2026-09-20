#include <iostream>
#include <string>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
// #include "spdlog/sinks/rotating_compress_file_sink.h"

#include <csignal>
#include <atomic>
#include <thread>
#include "cfgparse.hpp"

// 全局运行标志，信号处理函数里置 false
std::atomic<bool> g_running{true};

void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

bool init_log(const DmcConfig &config)
{
    try
    {
        std::vector<spdlog::sink_ptr> sinks;
        if (config.log_type == "console" || config.log_type == "all")
        {
            // 创建控制台输出槽
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(config.log_level);
            console_sink->set_pattern("[%H:%M:%S] [%^%l%$] [%@] [process %P:thread %t] %v");
            sinks.push_back(console_sink);
        }
        if (config.log_type == "file" || config.log_type == "all")
        {
            // 创建文件输出槽
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file_full_name,
                config.log_file_maxsize,
                config.log_file_cycles, false);
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
    catch (const spdlog::spdlog_ex &exception)
    {
        std::cout << "[ERROR] Log initialization failed: " << exception.what() << std::endl;
        return false;
    }
    catch (const std::exception &exception)
    {
        std::cout << "[ERROR] An error occurred during log initialization: " << exception.what() << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    // 注册 Ctrl+C (SIGINT) 和终止信号
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    int ret = 0;
    const char *persistPath = "./config/config_or.default";
    const char *jsonpersistPath = "./config/config.json";
    DmcConfig dmc_cfg;
    AppConfig app_cfg;
    JsonAppConfig json_app_cfg;
    // =========创建对象后，只读取一次文件到内存=========
    if (argc < 2)
    {
        std::cout << "Usage:\n"
                  << "[your.exe]" << " toml\n"
                  << "[your.exe]" << " json\n"
                  << "[your.exe]" << " jsontest" << std::endl;
        return -1;
    }

    if (argv[1] == std::string("toml"))
    {
        std::cout << "[INFO] This is toml branch" << std::endl;
        app_cfg.load_from_file(persistPath);
        if (!app_cfg.GetparseStatus())
        {
            std::cout << "[ERROR] Parse tomlfile failed" << std::endl;
            return -1;
        }
        // 后续解析直接复用内存里的app_cfg对象，不再读磁盘
        app_cfg.Config_Logsetting(app_cfg, dmc_cfg);
    }

    else if (argv[1] == std::string("json"))
    {
        std::cout << "[INFO] This is json branch" << std::endl;
        json_app_cfg.loadJson_from_file(jsonpersistPath);
        if (!json_app_cfg.GetparseStatus())
        {
            std::cout << "[ERROR] Parse json failed" << std::endl;
            return -1;
        }
        // 后续解析直接复用内存里的app_cfg对象，不再读磁盘
        json_app_cfg.JsonConfig_Logsetting(json_app_cfg, dmc_cfg);
    }

    else if (argv[1] == std::string("jsontest"))
    {
        std::cout << "[INFO] This is json test branch" << std::endl;
        json_app_cfg.loadJson_from_file(jsonpersistPath);
        if (!json_app_cfg.GetparseStatus())
        {
            std::cout << "[ERROR] Parse json test failed" << std::endl;
            return -1;
        }
        // 后续解析直接复用内存里的app_cfg对象，不再读磁盘
        json_app_cfg.JsonConfig_Logsetting_test(json_app_cfg, dmc_cfg);
    }

    else
    {
        std::cout << "Usage:\n"
                  << "[your.exe]" << " toml"
                  << "[your.exe]" << " json"
                  << "[your.exe]" << " jsontest"
                  << std::endl;
        return -1;
    }

    if (!init_log(dmc_cfg))
    {
        std::cout << "[ERROR] Log initialization failed" << std::endl;
        return -1;
    }
    std::cout << dmc_cfg.log_file_full_name << std::endl;
    std::cout << dmc_cfg.log_file_cycles << std::endl;
    std::cout << dmc_cfg.log_file_maxsize << std::endl;
    std::cout << dmc_cfg.log_level << std::endl;

    while (g_running)
    {

        SPDLOG_INFO("retnum: {}", ret);
        SPDLOG_WARN("retnum: {}", ret + 1);
        SPDLOG_ERROR("retnum: {}", ret + 2);
        SPDLOG_DEBUG("retnum: {}", ret + 2);
        // 加一点延时，避免疯狂写日志导致 I/O 阻塞、CPU 占满
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // dmc_cfg.test = toml::find_or<std::string>(app_cfg.document(),"app", "log_file_full_name", "/tmp/logs/app.log");
    // 优雅退出：刷新所有缓冲区、关闭文件句柄
    spdlog::shutdown();

    return 0;
}