#ifndef CFGPARSE_HPP
#define CFGPARSE_HPP
#include "toml/toml.hpp"
#include <iostream>
#include <string>
#include "spdlog/spdlog.h"
#include "json/json.hpp"

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

    const bool GetparseStatus() const

    {
        return Appparse;
    }

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

    bool load_from_file(const char *persistPath)
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
        catch (const toml::syntax_error &e)
        {
            std::cout << "[ERROR] TOML syntax error: " << e.what() << std::endl;
            return false;
        }
        catch (const toml::file_io_error &e)
        {
            std::cout << "[ERROR] Config file I/O error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception &e)
        {
            std::cout << "[ERROR] Exception while loading config: " << e.what() << std::endl;
            return false;
        }
        Appparse = true;

        return true;
    }

    void Config_Logsetting(const AppConfig &app_cfg, DmcConfig &cfg)
    {
        const toml::value &data = app_cfg.document();
        // ---- dmc ----
        if (data.contains("dmc"))
        {
            const auto &t = data.at("dmc");
            cfg.log_file_full_name = toml::find_or<std::string>(t, "log_file_full_name", "./logs/app.log");
            cfg.log_file_cycles = toml::find_or<int>(t, "log_file_cycles", 10);
            cfg.log_file_maxsize = toml::find_or<int>(t, "log_file_maxsize", 10485760);
            cfg.log_type = toml::find_or<std::string>(t, "log_type", "all");
            const std::string Slog_level = toml::find_or<std::string>(t, "log_level", "info");

            if (Slog_level == "debug")
            {
                cfg.log_level = spdlog::level::debug;
            }
            else if (Slog_level == "info")
            {
                cfg.log_level = spdlog::level::info;
            }
            else if (Slog_level == "warn")
            {
                cfg.log_level = spdlog::level::warn;
            }
            else if (Slog_level == "error")
            {
                cfg.log_level = spdlog::level::err;
            }
            else
            {
                cfg.log_level = spdlog::level::info;
            }
        }
        else
        {
            cfg.log_file_full_name = "./logs/app.log";
            cfg.log_file_cycles = 10;
            cfg.log_file_maxsize = 10485760;
            cfg.log_level = spdlog::level::info;
            cfg.log_type = "all";
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
    bool Appparse = false;
};

class JsonAppConfig
{
public:
    const nlohmann::json &document() const
    {
        return Jsondoc;
    }

    const bool GetparseStatus() const
    {
        return Jsonparse;
    }

    void JsonConfig_Logsetting(const JsonAppConfig &Jsonapp_cfg, DmcConfig &cfg)
    {
        const nlohmann::json &data = Jsonapp_cfg.document();
        if (data.contains("dmc"))
        {
            const auto &t = data.at("dmc");
            cfg.log_file_full_name = t.value<std::string>("log_file_full_name", "./logs/app.log");
            cfg.log_file_cycles = t.value<int>("log_file_cycles", 10);
            cfg.log_file_maxsize = t.value<int>("log_file_maxsize", 10485760);
            cfg.log_type = t.value<std::string>("log_type", "all");
            const std::string Slog_level = t.value<std::string>("log_level", "info");

            if (Slog_level == "debug")
            {
                cfg.log_level = spdlog::level::debug;
            }
            else if (Slog_level == "info")
            {
                cfg.log_level = spdlog::level::info;
            }
            else if (Slog_level == "warn")
            {
                cfg.log_level = spdlog::level::warn;
            }
            else if (Slog_level == "error")
            {
                cfg.log_level = spdlog::level::err;
            }
            else
            {
                cfg.log_level = spdlog::level::info;
            }
        }
        else
        {
            cfg.log_file_full_name = "./logs/app.log";
            cfg.log_file_cycles = 10;
            cfg.log_file_maxsize = 10485760;
            cfg.log_level = spdlog::level::info;
            cfg.log_type = "all";
        }
    }

    void JsonConfig_Logsetting_test(const JsonAppConfig &Jsonapp_cfg, DmcConfig &cfg)
    {
        const nlohmann::json &data = Jsonapp_cfg.document();
        if (data.contains("test"))
        {
            const auto &t = data.at("test");
            // cfg.log_file_full_name = t.value<std::string>("log_file_full_name", "./logs/app.log").at(0);
            auto log_arr = t.value<std::vector<std::string>>("log_file_full_name", std::vector<std::string>{"./logs/app.log", "./logs/test.log"});
            cfg.log_file_full_name = log_arr.empty() ? "./logs/app.log" : log_arr[1];
            cfg.log_file_cycles = t.value<int>("log_file_cycles", 10);
            cfg.log_file_maxsize = t.value<int>("log_file_maxsize", 10485760);
            cfg.log_type = t.value<std::string>("log_type", "all");
            const std::string Slog_level = t.value<std::string>("log_level", "info");

            if (Slog_level == "debug")
            {
                cfg.log_level = spdlog::level::debug;
            }
            else if (Slog_level == "info")
            {
                cfg.log_level = spdlog::level::info;
            }
            else if (Slog_level == "warn")
            {
                cfg.log_level = spdlog::level::warn;
            }
            else if (Slog_level == "error")
            {
                cfg.log_level = spdlog::level::err;
            }
            else
            {
                cfg.log_level = spdlog::level::info;
            }
        }
        else
        {
            cfg.log_file_full_name = "./logs/app.log";
            cfg.log_file_cycles = 10;
            cfg.log_file_maxsize = 10485760;
            cfg.log_level = spdlog::level::info;
            cfg.log_type = "all";
        }
    }

    bool loadJson_from_file(const char *persistPath)
    {
        // Call external function for file system validation
        if (!CheckJsonConfigFile(persistPath))
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
            // Read and parse at one‑shot, including JSON syntax validation
            Jsondoc = nlohmann::json::parse(fs);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            std::cout << "[ERROR] JSON syntax error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception &e)
        {
            std::cout << "[ERROR] Exception while loading config: " << e.what() << std::endl;
            return false;
        }
        Jsonparse = true;

        return true;
    }

private:
    bool CheckJsonConfigFile(const char *const persistPath)
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
    nlohmann::json Jsondoc;
    bool Jsonparse = false;
};

#endif