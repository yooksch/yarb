#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <format>
#include <chrono>

template <typename T>
concept Formattable = requires (T value) {
    std::format("", value);
};

class Log {
public:
    static const int LEVEL_INFO = 0;
    static const int LEVEL_WARNING = 1;
    static const int LEVEL_ERROR = 2;
    static const int LEVEL_DEBUG = 3;

    template<Formattable... T>
    static void Info(const char* ident, std::format_string<T...> fmt, T&&... args) {
        Println("INFO", ident, std::format(fmt, std::forward<T>(args)...));
    }

    template<Formattable... T>
    static void Warning(const char* ident, std::format_string<T...> fmt, T&&... args) {
        if (log_level < Log::LEVEL_WARNING) return;
        Println("WARNING", ident, std::format(fmt, std::forward<T>(args)...));
    }

    template<Formattable... T>
    static void Error(const char* ident, std::format_string<T...> fmt, T&&... args) {
        if (log_level < Log::LEVEL_ERROR) return;
        Println("ERROR", ident, std::format(fmt, std::forward<T>(args)...));
    }

    template<Formattable... T>
    static void Debug(const char* ident, std::format_string<T...> fmt, T&&... args) {
        if (log_level < Log::LEVEL_DEBUG) return;
        Println("DEBUG", ident, std::format(fmt, std::forward<T>(args)...));
    }

    static void SetLevel(int level) {
        assert(level >= 0 && level <= 3);
        log_level = level;
    }

    static void AllocWinConsole();
    static void FreeWinConsole();
    
    static void OpenLogFile();
    static void FreeLogFile();
private:
    static std::unique_ptr<std::ofstream> log_file;
    static int log_level;

    template<Formattable... T>
    static void Println(const char* mode, const char* ident, std::string msg) {
        std::string formatted_time = std::format("{:%T}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));

        msg = std::format("[{}] [{}] [{}] {}", formatted_time, ident, mode, msg);
    
        std::cout << msg << "\n";
        if (log_file != nullptr) {
            *log_file << msg << std::endl;
        }
    }
};