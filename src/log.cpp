#include "log.hpp"
#include "paths.hpp"
#include <Windows.h>
#include <cstdio>
#include <fstream>
#include <memory>

std::unique_ptr<std::ofstream> Log::log_file = nullptr;
int Log::log_level = Log::LEVEL_ERROR;

void Log::AllocWinConsole() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
}

void Log::FreeWinConsole() {
    FreeConsole();
}

void Log::OpenLogFile() {
    log_file = std::make_unique<std::ofstream>(Paths::LogFile);
}

void Log::FreeLogFile() {
    log_file->flush();
    log_file->close();
}