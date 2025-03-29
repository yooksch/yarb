#include "game.hpp"
#include "config.hpp"
#include "discordrpc.hpp"
#include "http.hpp"
#include "crypto.hpp"
#include "log.hpp"
#include "modmanager.hpp"
#include "paths.hpp"
#include "winhelpers.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <iterator>
#include <processthreadsapi.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <stringapiset.h>
#include <thread>
#include <TlHelp32.h>
#include <zip.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Game {
    const char* CDN_URL = "https://setup.rbxcdn.com";

    const std::map<std::string, std::string> PACKAGE_MAP = {
        { "Libraries.zip", ".\\" },
        { "shaders.zip", "shaders\\" },
        { "redist.zip", ".\\" },
        { "ssl.zip", "ssl\\" },
        { "WebView2.zip", ".\\" },
        { "WebView2RuntimeInstaller.zip", "WebView2RuntimeInstaller\\" },
        { "content-avatar.zip", "content\\avatar\\" },
        { "content-configs.zip", "content\\configs\\" },
        { "content-fonts.zip", "content\\fonts\\" },
        { "content-sky.zip", "content\\sky\\" },
        { "content-sounds.zip", "content\\sounds\\" },
        { "content-textures2.zip", "content\\textures\\" },
        { "content-models.zip", "content\\models\\" },
        { "content-textures3.zip", "PlatformContent\\pc\\textures\\" },
        { "content-terrain.zip", "PlatformContent\\pc\\terrain\\" },
        { "content-platform-fonts.zip", "PlatformContent\\pc\\fonts\\" },
        { "content-platform-dictionaries.zip", "PlatformContent\\pc\\shared_compression_dictionaries\\" },
        { "extracontent-luapackages.zip", "ExtraContent\\LuaPackages\\" },
        { "extracontent-translations.zip", "ExtraContent\\translations\\" },
        { "extracontent-models.zip", "ExtraContent\\models\\" },
        { "extracontent-textures.zip", "ExtraContent\\textures\\" },
        { "extracontent-places.zip", "ExtraContent\\places\\" },
        { "RobloxApp.zip", ".\\" }
    };

    std::string GetLatestRobloxVersion() {
        auto res = Http::Get("https://clientsettingscdn.roblox.com/v2/client-version/WindowsPlayer/channel/live");
        if (res.status_code != 200) throw std::exception("Failed to get latest roblox version");

        json j = json::parse(res.text);
        return j["clientVersionUpload"];
    }

    std::vector<ManifestEntry> GetManifest(std::string& version) {
        auto res = Http::Get(std::format("{}/{}-rbxPkgManifest.txt", CDN_URL, version).c_str());
        if (res.status_code != 200) throw std::exception("Failed to get manifest");

        std::istringstream stream(res.text);
        std::string v;
        
        std::getline(stream, v);
        v.pop_back();
        if (v != "v0") {
            throw std::exception("Unsupported manifest version");
        }

        std::vector<ManifestEntry> entries;
        std::string name, signature, packed_size, size;
        while (std::getline(stream, name)) {
            std::getline(stream, signature);
            std::getline(stream, packed_size);
            std::getline(stream, size);

            // strip carriage returns
            name.pop_back();
            signature.pop_back();
            packed_size.pop_back();
            signature.pop_back();

            entries.emplace_back(ManifestEntry {
                .name = name,
                .signature = signature,
                .packed_size = std::stoull(packed_size),
                .size = std::stoull(size)
            });
        }

        return entries;
    }

    void Download(const std::string& version, const std::vector<ManifestEntry>& manifest, bool efficient_download, std::function<void(size_t)> progress_callback) {
        std::vector<std::thread> threads;
        size_t packages_installed = 0;
        for (const ManifestEntry& entry : manifest) {
            if (entry.name == "RobloxPlayerLauncher.exe") continue;

            // Skip package which didn't change
            if (efficient_download && PACKAGE_SIGNATURES.contains(entry.name)) {
                if (PACKAGE_SIGNATURES[entry.name] == entry.signature) {
                    Log::Debug("Game::Download", "Skipped {} - Signatures are identical", entry.name);
                    continue;
                }
            }

            threads.emplace_back(std::thread([&] {
                try {
                    DownloadPackage(version, entry);
                } catch (std::exception ex) {
                    Log::Error("Game::Download", "Failed to install package {}", std::string(entry.name));
                }
                progress_callback(++packages_installed);
            }));
        }

        for (auto& t : threads) {
            t.join();
        }

        SaveSignatures();

        // Write AppSettings.xml
        const char* app_settings_xml = R"(
        <?xml version="1.0" encoding="UTF-8"?>
<Settings>
	<ContentFolder>content</ContentFolder>
	<BaseUrl>http://www.roblox.com</BaseUrl>
</Settings>
        )";
        std::ofstream stream(std::filesystem::path(Paths::GameDirectory).append("AppSettings.xml"));
        stream.write(app_settings_xml, strlen(app_settings_xml));

        Config::GetInstance()->installed_version = version;
        Log::Info("Game::Download", "Downloaded Roblox {}", version);
    }

    void DownloadPackage(const std::string &version, const ManifestEntry &package) {
        auto res = Http::Get(std::format("{}/{}-{}", CDN_URL, version, package.name).c_str());
        if (!PACKAGE_MAP.contains(package.name)) return;
        auto path = Paths::GameDirectory / PACKAGE_MAP.at(package.name);

        // extract zip
        zip_error_t err;
        zip_source_t* src = zip_source_buffer_create(res.bytes.data(), res.bytes.size(), 0, &err);

        if (src == nullptr) {
            throw std::exception(std::format("Error creating zip buffer: {}", zip_error_strerror(&err)).c_str());
        }

        zip_t* archive = zip_open_from_source(src, 0, &err);

        if (archive == nullptr) {
            throw std::exception(std::format("Error opening zip: {}", zip_error_strerror(&err)).c_str());
        }

        zip_int64_t num_entries = zip_get_num_entries(archive, 0);
        for (zip_int64_t i = 0; i < num_entries; i++) {
            zip_stat_t stat;
            if (zip_stat_index(archive, i, 0, &stat) == 0) {
                zip_file_t* file = zip_fopen_index(archive, i, 0);
                if (file != nullptr) {
                    std::vector<char> content(stat.size);
                    zip_fread(file, content.data(), stat.size);
                    zip_fclose(file);

                    auto name = std::string(stat.name);
                    if (name.starts_with("\\"))
                        name = name.substr(1);
                    
                    auto file_path = path / name;
                    std::filesystem::create_directories(file_path.parent_path());

                    FILE_SIGNATURES[file_path.string()] = FileSignature {
                        .sha256 = Crypto::ComputeSHA256(content),
                        .origin_package = package.name
                    };

                    std::ofstream ofstream(file_path, std::ios::binary);
                    ofstream.write(content.data(), content.size());
                    ofstream.close();
                }
            }
        }

        zip_close(archive);
        zip_source_close(src);

        // Save package signature to PACKAGE_HASHES
        PACKAGE_SIGNATURES[package.name] = package.signature;

        Log::Debug("Game::Download", "Downloaded package {}", package.name);
    }

    bool Start(std::string args, bool safe_mode) {
        Log::Info("Game::Start", "Starting Roblox...");
        // Check if roblox is already running
        if (Config::GetInstance()->prevent_multi_launch) {
            PROCESSENTRY32 pe;
            pe.dwSize = sizeof(PROCESSENTRY32);
            HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

            if (Process32First(handle, &pe)) {
                while (Process32Next(handle, &pe)) {
                    if (std::string(pe.szExeFile) == "RobloxPlayerBeta.exe") {
                        Log::Info("Game::Start", "Roblox is already running. Aborting");
                        return false;
                    }
                }
            }
        }

        if (!safe_mode) {
            ModManager::GetInstance()->ApplyModifications();
            ModManager::GetInstance()->ApplyFastFlags();
        } else {
            ModManager::GetInstance()->RemoveFastFlags();
        }

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmd = std::format("\"{}\" {}", (Paths::GameDirectory / "RobloxPlayerBeta.exe").string(), args);

        if (!CreateProcess(
            NULL,
            reinterpret_cast<LPSTR>(cmd.data()),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            reinterpret_cast<LPSTR>(Paths::GameDirectory.string().data()),
            &si,
            &pi
        )) {
            Log::Error("Game::Start", "Failed to start process: {}", GetLastError());
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        Log::Info("Game::Start", "Roblox exited with code {}", exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (!safe_mode) {
            ModManager::GetInstance()->RestoreOriginalFiles();
            ModManager::GetInstance()->RemoveFastFlags();
        }
        return true;
    }

    void Bootstrap(bool efficient_download, std::function<void (BootstrapStatusUpdate)> callback) {
        callback(BootstrapStatusUpdate { GettingVersion });
        auto version = GetLatestRobloxVersion();
        if (version == Config::GetInstance()->installed_version) {
            Log::Info("Game::Bootstrap", "Roblox is up to date");
            if (Config::GetInstance()->verify_integrity_on_launch) {
                VerifyFileIntegrity([&](size_t current, size_t total) {
                    callback(BootstrapStatusUpdate { VerifyingFileIntegrity, current, total });
                });
            }
        } else {
            callback(BootstrapStatusUpdate { GettingManifest });
            auto manifest = GetManifest(version);
            callback(BootstrapStatusUpdate { DownloadingPackages, 0, manifest.size() });
            Download(version, manifest, efficient_download, [&](size_t p) {
                callback(BootstrapStatusUpdate { DownloadingPackages, p, manifest.size() });
            });
        }

        callback(BootstrapStatusUpdate { Complete });
    }

    void VerifyFileIntegrity(std::function<void(size_t, size_t)> progress_callback) {
        std::vector<std::filesystem::path> files;
        auto walk_dir = [&](this auto&& self, const std::filesystem::path& path) -> void {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.emplace_back(entry.path());
                } else if (entry.is_directory()) {
                    self(entry.path());
                }
            }
        };

        Log::Info("Game::VerifyFileIntegrity", "Indexing game files...");
        walk_dir(Paths::GameDirectory);

        Log::Info("Game::VerifyFileIntegrity", "Verifying {} files...", files.size());
        progress_callback(0, files.size());
        size_t progress = 0;
        for (const auto& path : files) {
            auto ps = path.string();
            if (FILE_SIGNATURES.contains(ps)) {
                std::ifstream file(ps, std::ios::binary);
                std::vector<char> file_content {
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                };
                auto actual_sig = Crypto::ComputeSHA256(file_content);
                auto expected_signature = FILE_SIGNATURES[ps];

                if (actual_sig != expected_signature.sha256) {
                    Log::Warning(
                        "Game::VerifyFileIntegrity",
                        "File {} is corrupt. Reinstalling source package",
                        std::filesystem::relative(path, Paths::GameDirectory).string()
                    );
                    DownloadPackage(Config::GetInstance()->installed_version, ManifestEntry {
                        .name = expected_signature.origin_package,
                        .signature = PACKAGE_SIGNATURES[expected_signature.origin_package],
                        .packed_size = 0,
                        .size = 0
                    });
                }
                progress_callback(++progress, files.size());
            }
        }

        Log::Info("Game::VerifyFileIntegrity", "Verification complete");
    }

    void LoadSavedSignatures() {
        PACKAGE_SIGNATURES.clear();
        FILE_SIGNATURES.clear();

        std::ifstream file(Paths::SignaturesFile);
        json j = json::parse(file);

        if (auto ps = j["package_signatures"]; ps.is_object()) {
            for (const auto& entry : ps.items()) {
                PACKAGE_SIGNATURES.emplace(entry.key(), entry.value());
            }
        }

        if (auto fs = j["file_signatures"]; fs.is_object()) {
            for (const auto& entry : fs.items()) {
                FILE_SIGNATURES.emplace(entry.key(), FileSignature {
                    .sha256 = entry.value()["sha256"],
                    .origin_package = entry.value()["origin_package"]
                });
            }
        }
    }

    void SaveSignatures() {
        json j;

        for (const auto& entry : PACKAGE_SIGNATURES) {
            j["package_signatures"][entry.first] = entry.second;
        }

        for (const auto& entry : FILE_SIGNATURES) {
            j["file_signatures"][entry.first] = {
                { "sha256", entry.second.sha256 },
                { "origin_package", entry.second.origin_package }
            };
        }

        std::ofstream file(Paths::SignaturesFile);
        file << j.dump(4);
    }

    /// Must be called before starting the game
    void WatchRobloxLog() {
        // Wait for a new log file to be created
        std::set<std::filesystem::path> existing_files;

        // Populate existing files
        for (const auto& entry : std::filesystem::directory_iterator(Paths::RobloxLogDirectory)) {
            if (!entry.is_regular_file()) continue;
            existing_files.emplace(entry.path());
        }

        // Check for new files
        std::filesystem::path log_path;
        const int MAX_ITERATIONS = 600; // equivalent to 1 minute
        int iteration = 0;
        while (iteration++ < MAX_ITERATIONS && log_path.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(Paths::RobloxLogDirectory)) {
                if (existing_files.contains(entry.path()) || !entry.is_regular_file()) continue;
                log_path = entry.path();
                break;
            }
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(100));
        }

        Log::Debug("Game::WatchRobloxLog", "Found latest log file: {}", log_path.string());

        // Read log file line by line
        std::ifstream log_file(log_path);
        std::string line;
        std::streampos previous_pos;
        while (true) {
            log_file.seekg(0, std::ios::end);
            std::streampos file_size = log_file.tellg();

            if (file_size > previous_pos) {
                // File was written to
                log_file.seekg(previous_pos);
                
                while (std::getline(log_file, line)) {
                    previous_pos = log_file.tellg();

                    if (line.contains(" serverId: ")) {
                        static const std::regex regex("serverId: (\\d+\\.)\\|");
                        std::smatch match;
                        if (!std::regex_search(line, match, regex)) {
                            Log::Error("Game::WatchRobloxLog", "Failed to get server ip address");
                            continue;
                        }

                        auto ip = match.str(1);
                        Log::Debug("Game::WatchRobloxLog", "Getting location of {}", ip);
                        auto response = Http::Get(std::format("https://ipinfo.io/{}/json", ip).c_str());
                        if (response.status_code != 200) {
                            Log::Error("Game::WatchRobloxLog", "Failed to get server location");
                            continue;
                        }

                        json j = json::parse(response.text);
                        server_location = std::format(
                            "{}, {}, {}",
                            static_cast<std::string>(j["city"]),
                            static_cast<std::string>(j["region"]),
                            static_cast<std::string>(j["country"])
                        );

                        Log::Info("Game::WatchRobloxLog", "Server location: {}", server_location);
                        WinHelpers::SendNotification(L"Server Location", WinHelpers::StringToWideString(server_location));
                    } else if (line.contains("FLog::GameJoinLoadTime")) {
                        static const std::regex regex("universeid:(\\d+)");
                        std::smatch match;
                        if (std::regex_search(line, match, regex)) {
                            int universe_id = std::stoi(match.str(1).c_str());
                            Log::Debug("Game::WatchRobloxLog", "Getting universe details for {}", universe_id);
                            auto details = GetUniverseDetails(universe_id);
                            Log::Info("Game::WatchRobloxLog", "Joined universe {}", details.name);
                            DiscordRPC::GetInstance()->SetActivity(details);
                        } else {
                            Log::Warning("Game::WatchRobloxLog", "Failed to get current place id");
                        }
                    } else if (line.contains("NetworkClient:Remove")) {
                        DiscordRPC::GetInstance()->ClearActivity();
                    }
                }
            } else {
                // Small delay to not put too much load on the CPU/disk
                std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(20));
            }

            // Clear EOF flag
            log_file.clear();
        }
    }

    RobloxUniverseDetails GetUniverseDetails(int place_id) {
        RobloxUniverseDetails details {};

        auto res = Http::Get(std::format("https://games.roblox.com/v1/games?universeIds={}", place_id).c_str());
        if (res.status_code != 200) {
            throw new std::exception("Failed to get universe details");
        }

        json j = json::parse(res.text);
        auto data = j["data"][0];

        details.name = data["name"];
        details.creator = data["creator"]["name"];

        auto thumbnail_res = Http::Get(
            std::format(
                "https://thumbnails.roblox.com/v1/games/icons?universeIds={}&returnPolicy=PlaceHolder&size=128x128&format=Png&isCircular=false",
                place_id
            ).c_str());

        if (thumbnail_res.status_code != 200) {
            Log::Error("Game::GetUniverseDetails", "Failed to get thumbnail (status code: {})", thumbnail_res.status_code);
            return details;
        }

        j = json::parse(thumbnail_res.text);
        details.cover_url = j["data"][0]["imageUrl"];

        return details;
    }
}