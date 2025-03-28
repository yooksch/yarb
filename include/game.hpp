#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Game {
    struct FileSignature {
        std::string sha256;
        std::string origin_package;
    };

    extern const char* CDN_URL;
    extern const std::map<std::string, std::string> PACKAGE_MAP;
    static std::map<std::string, std::string> PACKAGE_SIGNATURES;
    static std::map<std::string, FileSignature> FILE_SIGNATURES;

    struct ManifestEntry {
        std::string name;
        std::string signature;
        unsigned long long packed_size;
        unsigned long long size;
    };

    enum BootstrapStatus {
        GettingVersion,
        GettingManifest,
        DownloadingPackages,
        VerifyingFileIntegrity,
        Complete
    };

    struct BootstrapStatusUpdate {
        BootstrapStatus status = GettingVersion;
        size_t progress_current = 0;
        size_t progress_max = 100;
    };

    std::string GetLatestRobloxVersion();
    std::vector<ManifestEntry> GetManifest(std::string& version);
    void Download(const std::string& version, const std::vector<ManifestEntry>& manifest, bool efficient_download, std::function<void(size_t)> progress_callback);
    void DownloadPackage(const std::string& version, const ManifestEntry& package);
    void RegisterProtocolHandler(const char* protocol, const std::filesystem::path& executable);
    bool Start(std::string args, bool safe_mode = false);
    void Bootstrap(bool efficient_download, std::function<void(BootstrapStatusUpdate)> callback);
    void VerifyFileIntegrity(std::function<void(size_t, size_t)> progress_callback);
    void LoadSavedSignatures();
    void SaveSignatures();
}