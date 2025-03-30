#include "discordrpc.hpp"
#include "drpc/drpc.hpp"
#include "game.hpp"
#include "log.hpp"

DiscordRPC* DiscordRPC::GetInstance() {
    static DiscordRPC instance;
    return &instance;
}

void DiscordRPC::Init() {
    init_time = time(NULL);
    client = std::make_shared<DiscordRichPresence::Client>(1355511418530697336);
    client->Connect();

    activity = DiscordRichPresence::Activity { };
    activity.SetClientId(1355511418530697336);
    activity.SetType(DiscordRichPresence::ActivityType::Playing);
    activity.SetName("Roblox");

    ClearActivity();
}

void DiscordRPC::SetActivity(const Game::RobloxUniverseDetails place) {
    auto assets = &activity.GetAssets();
    assets->SetLargeImage(place.cover_url);
    assets->SetLargeImageText(place.name);

    activity.SetDetails(place.name);
    activity.SetState(std::format("by {}", place.creator));
    activity.GetTimestamps().SetStart(time(NULL));

    if (auto result = client->UpdateActivity(activity); result == DiscordRichPresence::Result::Ok) {
        Log::Info("DiscordRPC::SetActivity", "Successfully cleared activity");
    } else {
        Log::Error("DiscordRPC::SetActivity", "Failed to clear activity ({})", (int)result);
    }
}

void DiscordRPC::ClearActivity() {
    auto assets = &activity.GetAssets();
    assets->SetLargeImage("roblox");
    assets->SetLargeImageText("");

    activity.SetDetails("");
    activity.SetState("");
    activity.GetTimestamps().SetStart(init_time);

    if (auto result = client->UpdateActivity(activity); result == DiscordRichPresence::Result::Ok) {
        Log::Info("DiscordRPC::ClearActivity", "Successfully cleared activity");
    } else {
        Log::Error("DiscordRPC::ClearActivity", "Failed to clear activity ({})", (int)result);
    }
}
