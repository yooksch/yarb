#include "discordrpc.hpp"
#include "config.hpp"
#include "drpc/drpc.hpp"
#include "game.hpp"
#include "log.hpp"
#include <thread>

DiscordRPC* DiscordRPC::GetInstance() {
    static DiscordRPC instance;
    return &instance;
}

void DiscordRPC::Init() {
    init_time = time(NULL);
    client = std::make_shared<DiscordRichPresence::Client>(1355511418530697336);

    client->SetEventCallback([](auto event) {
        if (event == DiscordRichPresence::Event::Connected)
            Log::Info("DRPC::Events", "Connected");
        else if (event == DiscordRichPresence::Event::Disconnected)
            Log::Info("DRPC::Events", "Disconnected");
    });

    bool debug_mode = Config::GetInstance()->debug_mode;
    client->SetLogCallback([debug_mode](auto result, auto level, auto message, auto _) {
        if (!debug_mode && level == DiscordRichPresence::LogLevel::Trace) return;

        switch (level) {
        case DiscordRichPresence::LogLevel::Info:
            Log::Info("DRPC::Log", "{}: {}", DiscordRichPresence::ResultToString(result), message);
            break;
        case DiscordRichPresence::LogLevel::Warn:
            Log::Warning("DRPC::Log", "{}: {}", DiscordRichPresence::ResultToString(result), message);
            break;
        case DiscordRichPresence::LogLevel::Error:
            Log::Error("DRPC::Log", "{}: {}", DiscordRichPresence::ResultToString(result), message);
            break;
        case DiscordRichPresence::LogLevel::Trace:
            Log::Debug("DRPC::Log", "{}: {}", DiscordRichPresence::ResultToString(result), message);
            break;
        }
    });

    Log::Info("DiscordRPC::Init", "Connecting");
    if (auto result = client->Connect(); result != DiscordRichPresence::Result::Ok)
        Log::Error("DiscordRPC::Init", "Failed to connect ({})", DiscordRichPresence::ResultToString(result));

    activity = std::make_shared<DiscordRichPresence::Activity>();
    activity->SetClientId(1355511418530697336);
    activity->SetType(DiscordRichPresence::ActivityType::Playing);
    activity->SetName("Roblox");

    SetInApp();
}

void DiscordRPC::SetInGame(const Game::RobloxUniverseDetails place) {
    auto assets = activity->GetAssets();
    assets->SetLargeImage(place.cover_url);
    assets->SetLargeImageText(place.name);

    activity->SetDetails(place.name);
    activity->SetState(std::format("by {}", place.creator));
    activity->GetTimestamps()->SetStart(std::time(nullptr));

    client->UpdateActivity(activity, [](auto result, auto) {
        if (result == DiscordRichPresence::Result::Ok)
            Log::Info("DiscordRPC::SetInGame", "{}", "Successfully set activity");
        else
            Log::Error("DiscordRPC::SetInGame", "{} ({})", "Failed to set activity", DiscordRichPresence::ResultToString(result));
    });
}

void DiscordRPC::SetInApp() {
    auto assets = activity->GetAssets();
    assets->SetLargeImage("roblox");
    assets->SetLargeImageText("");

    activity->SetDetails("");
    activity->SetState("");
    activity->GetTimestamps()->SetStart(init_time);

    client->UpdateActivity(activity, [](auto result, auto) {
        if (result == DiscordRichPresence::Result::Ok)
            Log::Info("DiscordRPC::SetInApp", "{}", "Successfully set activity");
        else
            Log::Error("DiscordRPC::SetInApp", "{} ({})", "Failed to set activity", DiscordRichPresence::ResultToString(result));
    });
}

void DiscordRPC::Start() {
    std::thread([this]() {
        auto result = client->Run();
        Log::Info("DRPC", "Exited with: {}", DiscordRichPresence::ResultToString(result));
    }).detach();
}
