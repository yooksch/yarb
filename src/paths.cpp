#include "paths.hpp"
#include <Windows.h>
#include <ShlObj_core.h>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <knownfolders.h>
#include <shtypes.h>
#include <winuser.h>

std::filesystem::path Paths::RootDirectory = ".\\yarb";
std::filesystem::path Paths::GameDirectory = RootDirectory / "Game";
std::filesystem::path Paths::ModsDirectory = RootDirectory / "Mods";
std::filesystem::path Paths::LogFile = RootDirectory / "latest.log";
std::filesystem::path Paths::ConfigFile = RootDirectory / "config.json";

void Paths::InitPaths() {
    PWSTR local_app_data;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, NULL, NULL, &local_app_data))) {
        RootDirectory = std::filesystem::path(local_app_data) / "yarb";
        GameDirectory = RootDirectory / "Game";
        ModsDirectory = RootDirectory / "Mods";
        LogFile = RootDirectory / "latest.log";
        ConfigFile = RootDirectory / "config.json";

        // Create directories
        std::filesystem::create_directories(GameDirectory);
        std::filesystem::create_directories(ModsDirectory);
    } else {
        MessageBoxExA(NULL, "Failed to find local appdata path", "yarb error", 0, 0);
        exit(1);
    }
}