#include "config.hpp"
#include "discordrpc.hpp"
#include "game.hpp"
#include "gui.hpp"
#include "log.hpp"
#include "paths.hpp"
#include "winhelpers.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <thread>
#include <windows.h>
#include <format>

namespace chrono = std::chrono;

std::vector<std::string> split(std::string string, const std::string& delimeter) {
    std::vector<std::string> result;
    std::string s;
    size_t pos;

    while ((pos = string.find(delimeter)) != std::string::npos) {
        s = string.substr(0, pos);
        result.push_back(s);
        string.erase(0, pos + delimeter.length());
    }
    result.push_back(string);

    return result;
}

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
int main(int argc, char** argv) {
    // Ensure paths are set and directories are created
    Paths::InitPaths();

    // Setup log
    Log::OpenLogFile();
    Log::SetLevel(Log::LEVEL_DEBUG);

    // Load config
    auto config = Config::GetInstance();
    config->Load(Paths::ConfigFile);
    if (config->debug_mode) {
        Log::SetLevel(Log::LEVEL_DEBUG);
        Log::AllocWinConsole();
        Log::Info("MAIN", "Started in debug mode");
    } else {
        Log::SetLevel(Log::LEVEL_ERROR);
    }

    // Load signatures
    try {
        if (std::filesystem::exists(Paths::SignaturesFile))
            Game::LoadSavedSignatures();
    } catch (std::exception ex) {
        Log::Error("MAIN", "Failed to load signatures from file ({})", ex.what());
    }

    // Register protocol handlers
    WinHelpers::RegisterProtocolHandler("roblox", argv[0]);
    WinHelpers::RegisterProtocolHandler("roblox-player", argv[0]);

    // Register AUMID for notifications
    WinHelpers::CreateUserAppModelIdEntry(argv[0]);

    std::string command;
    if (argc < 2)
        command = "gui";
    else
        command = std::string(argv[1]);

    // Convert command into lowercase
    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (command == "update") {
        auto version = Game::GetLatestRobloxVersion();
        if (config->installed_version == version) {
            std::cout << "Roblox is up to date!" << std::endl;
        } else {
            Log::Info("MAIN", "Roblox update found. Installing Roblox {}", version);
            auto manifest = Game::GetManifest(version);
            Game::Download(version, manifest, config->efficient_download, [](int _){});
        }
    } else if (command == "gui") {
        SettingsGUI gui(700, 500);
        gui.Show();
    } else  if (command == "launch") {
        std::string payload = "--app";
        if (argc >= 3) {
            payload = argv[2];
        }

        if (payload != "--app") {
            std::string launch_args;

            if (payload.starts_with("roblox-player:")) {
                std::string new_payload;
                for (const auto& s : split(payload, "+")) {
                    auto parts = split(s, ":");
                    std::string key = parts[0];
                    std::string value = parts[1];

                    if (key == "launchtime") {
                        auto timestamp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
                        value = std::to_string(timestamp);
                    } else if (key == "channel") {
                        continue;
                    }

                    new_payload += std::format("{}:{}+", key, value);
                }
                new_payload.erase(new_payload.length());
                payload = new_payload;
            } else if (payload.starts_with("roblox:")) {
                launch_args = std::format("--app --deeplink {}", payload);
            } else {
                Log::Error("MAIN", "Payload is invalid");
                goto exit;
            }
        }

        {
            LaunchGUI gui(350, 100);

            std::thread([&] {
                Game::Bootstrap(config->efficient_download, [&](Game::BootstrapStatusUpdate update) {
                    gui.status = update.status;
                    gui.progress_current = update.progress_current;
                    gui.progress_max = update.progress_max;
                });
            }).detach();

            gui.Show();
        }

        // start log watcher thread only if it's needed
        if (config->query_server_location) {
            std::thread([] {
                Game::WatchRobloxLog();
            }).detach();
        }

        if (config->discord_rpc) {
            DiscordRPC::GetInstance()->Init();
            DiscordRPC::GetInstance()->StartThread();
        }

        Game::Start(payload);
    }

    exit:
    Config::GetInstance()->Save(Paths::ConfigFile);
    Log::FreeWinConsole();
    Log::FreeLogFile();
    return 0;
}