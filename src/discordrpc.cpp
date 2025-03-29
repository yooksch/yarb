#include "discordrpc.hpp"
#include "game.hpp"
#include "log.hpp"
#include "types.h"

#include <csignal>
#include <thread>

DiscordRPC* DiscordRPC::GetInstance() {
    static DiscordRPC instance;
    return &instance;
}

void DiscordRPC::Init() {
    init_time = time(NULL);
    auto result = discord::Core::Create(1355511418530697336, DiscordCreateFlags_Default, &core);
    if (!core) {
        Log::Error("DiscordRPC::Init", "Failed to create core: {}", static_cast<int>(result));
        return;
    }

    core->SetLogHook(discord::LogLevel::Info, [](discord::LogLevel level, const char* msg) {
        switch (level) {
        case discord::LogLevel::Info:
            Log::Info("DiscordRPC::Init", "[DiscordSDK] {}", msg);
            break;
        case discord::LogLevel::Warn:
            Log::Warning("DiscordRPC::Init", "[DiscordSDK] {}", msg);
            break;
        case discord::LogLevel::Error:
            Log::Error("DiscordRPC::Init", "[DiscordSDK] {}", msg);
            break;
        case discord::LogLevel::Debug:
            Log::Debug("DiscordRPC::Init", "[DiscordSDK] {}", msg);
            break;
        }
    });

    activity = discord::Activity { };
    activity.SetApplicationId(1355511418530697336);
    activity.SetType(discord::ActivityType::Playing);
    activity.SetName("Roblox");

    ClearActivity();
}

void DiscordRPC::SetActivity(const Game::RobloxUniverseDetails place) {
    auto assets = &activity.GetAssets();
    assets->SetLargeImage(place.cover_url.c_str());
    assets->SetLargeText(place.name.c_str());

    activity.SetDetails(std::format("Playing {}", place.name).c_str());
    activity.GetTimestamps().SetStart(time(NULL));

    core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
        if (result == discord::Result::Ok)
            Log::Info("DiscordRPC::SetActivity", "Updated activity");
        else
            Log::Info("DiscordRPC::SetActivity", "Failed to update activity");
    });
}

void DiscordRPC::ClearActivity() {
    auto assets = &activity.GetAssets();
    assets->SetLargeImage("roblox");
    assets->SetLargeText("");

    activity.SetDetails("");
    activity.GetTimestamps().SetStart(init_time);

    core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
        if (result == discord::Result::Ok)
            Log::Info("DiscordRPC::ClearActivity", "Updated activity");
        else
            Log::Info("DiscordRPC::ClearActivity", "Failed to update activity");
    });
}

void DiscordRPC::StartThread() {
    if (running || !core) return;
    running = true;

    static bool interrupt = false;
    std::signal(SIGINT, [](int) { interrupt = true; });

    std::thread([&] {
        while (!interrupt) {
            core->RunCallbacks();

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        interrupt = false;
        running = false;
    }).detach();
}