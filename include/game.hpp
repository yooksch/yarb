#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Game {
    extern const char* CDN_URL;
    extern const std::map<std::string, std::string> PACKAGE_MAP;

    typedef struct {
        std::string name;
        std::string signature;
        unsigned long long packed_size;
        unsigned long long size;
    } ManifestEntry;

    std::string GetLatestRobloxVersion();
    std::vector<ManifestEntry> GetManifest(std::string& version);
    void Download(const std::string& version, const std::vector<ManifestEntry>& manifest, const std::filesystem::path& install_dir, void (*progress_callback)(int));
    void RegisterProtocolHandler(const char* protocol, const std::filesystem::path& executable);
    bool Start(std::string args, bool safe_mode = false);
}