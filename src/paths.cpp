#include "paths.hpp"
#include <Windows.h>
#include <ShlObj_core.h>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <knownfolders.h>
#include <shtypes.h>
#include <winuser.h>

std::filesystem::path Paths::RootDirectory;
std::filesystem::path Paths::GameDirectory;
std::filesystem::path Paths::ModsDirectory;
std::filesystem::path Paths::LogFile;
std::filesystem::path Paths::ConfigFile;
std::filesystem::path Paths::SignaturesFile;
std::filesystem::path Paths::RobloxLogDirectory;

void Paths::InitPaths() {
    PWSTR local_app_data;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, NULL, NULL, &local_app_data))) {
        RootDirectory = std::filesystem::path(local_app_data) / "yarb";
        GameDirectory = RootDirectory / "Game";
        ModsDirectory = RootDirectory / "Mods";
        LogFile = RootDirectory / "latest.log";
        ConfigFile = RootDirectory / "config.json";
        SignaturesFile = RootDirectory / "hashes.json";
        RobloxLogDirectory = std::filesystem::path(local_app_data) / "Roblox" / "logs";

        // Create directories
        std::filesystem::create_directories(GameDirectory);
        std::filesystem::create_directories(ModsDirectory);
    } else {
        MessageBoxExA(NULL, "Failed to find local appdata path", "yarb error", 0, 0);
        exit(1);
    }
}