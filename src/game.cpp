#include "game.hpp"
#include "config.hpp"
#include "http.hpp"
#include "crypto.hpp"
#include "log.hpp"
#include "modmanager.hpp"
#include "paths.hpp"
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <processthreadsapi.h>
#include <sstream>
#include <string>
#include <thread>
#include <winnt.h>
#include <winreg.h>
#include <windows.h>
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

    void Download(const std::string& version, const std::vector<ManifestEntry>& manifest, const std::filesystem::path& install_dir, bool efficient_download, void (*progress_callback)(int)) {
        std::vector<std::thread> threads;
        int packages_installed = 0;
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
                auto res = Http::Get(std::format("{}/{}-{}", CDN_URL, version, entry.name).c_str());
                if (!PACKAGE_MAP.contains(entry.name)) return;
                auto path = install_dir / PACKAGE_MAP.at(entry.name);

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
                                .origin_package = entry.name
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
                PACKAGE_SIGNATURES[entry.name] = entry.signature;

                progress_callback(++packages_installed);
                Log::Debug("Game::Download", "Downloaded package {}", entry.name);
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
        std::ofstream stream(std::filesystem::path(install_dir).append("AppSettings.xml"));
        stream.write(app_settings_xml, strlen(app_settings_xml));

        Config::GetInstance()->installed_version = version;
        Log::Info("Deployment::Download", "Downloaded Roblox {}", version);
    }

    void RegisterProtocolHandler(const char *protocol, const std::filesystem::path &executable) {
        HKEY key {};
        LRESULT result = RegCreateKeyExA(
            HKEY_CURRENT_USER,
            std::format("Software\\Classes\\{}", protocol).c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            NULL,
            &key,
            NULL
        );
        if (result != ERROR_SUCCESS) {
            Log::Error("Deployment::RegisterProtocolHandler", "Failed to register protocol handler");
            return;
        }

        std::string value = "URL: Roblox Protocol";
        RegSetValueExA(
            key,
            "",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            (value.length() + 1) * sizeof(wchar_t)
        );
        RegSetValueExA(
            key,
            "URL Protocol",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(""),
            1
        );
        RegCloseKey(key);

        result = RegCreateKeyExA(
            HKEY_CURRENT_USER,
            std::format("Software\\Classes\\{}\\DefaultIcon", protocol).c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            NULL,
            &key,
            NULL
        );
        if (result != ERROR_SUCCESS) {
            Log::Error("Deployment::RegisterProtocolHandler", "Failed to register protocol handler");
            return;
        }

        value = executable.string();
        RegSetValueExA(
            key,
            "",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            (value.length() + 1) * sizeof(wchar_t)
        );
        RegCloseKey(key);

        result = RegCreateKeyExA(
            HKEY_CURRENT_USER,
            std::format("Software\\Classes\\{}\\shell\\open\\command", protocol).c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            NULL,
            &key,
            NULL
        );
        if (result != ERROR_SUCCESS) {
            Log::Error("Deployment::RegisterProtocolHandler", "Failed to register protocol handler");
            return;
        }

        value = std::format("\"{}\" launch %1", executable.string());
        RegSetValueExA(
            key,
            "",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            (value.length() + 1) * sizeof(wchar_t)
        );
        RegCloseKey(key);
    }

    bool Start(std::string args, bool safe_mode) {
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
}