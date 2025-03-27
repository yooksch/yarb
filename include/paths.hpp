#pragma once

#include <filesystem>

class Paths {
public:
    static std::filesystem::path RootDirectory;
    static std::filesystem::path GameDirectory;
    static std::filesystem::path ModsDirectory;
    static std::filesystem::path LogFile;
    static std::filesystem::path ConfigFile;

    static void InitPaths();
};