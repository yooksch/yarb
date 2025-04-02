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
    if (client->Connect() == DiscordRichPresence::Result::Ok)
        Log::Info("DiscordRPC::Init", "Connected");
    else
        Log::Error("DiscordRPC::Init", "Failed to connect");

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

    update_activity:
    switch (client->UpdateActivity(activity)) {
    case DiscordRichPresence::Result::Ok:
        Log::Info("DiscordRPC::SetActivity", "Successfully set activity");
        break;
    case DiscordRichPresence::Result::WritePipeFailed:
        // Pipe handle was most likely closed, try reconnecting
        Log::Error("DiscordRPC::SetActivity", "Failed to set activity (WritePipeFailed)");
        if (client->Reconnect() == DiscordRichPresence::Result::Ok) {
            Log::Info("DiscordRPC::SetActivity", "Reconnected to IPC, retrying...");
            goto update_activity;
        } else {
            Log::Error("DiscordRPC::SetActivity", "Failed to reconnect to IPC");
        }
        break;
    default:
        Log::Error("DiscordRPC::SetActivity", "Failed to set activity");
        break;
    }
}

void DiscordRPC::ClearActivity() {
    auto assets = &activity.GetAssets();
    assets->SetLargeImage("roblox");
    assets->SetLargeImageText("");

    activity.SetDetails("");
    activity.SetState("");
    activity.GetTimestamps().SetStart(init_time);

    update_activity:
    switch (client->UpdateActivity(activity)) {
    case DiscordRichPresence::Result::Ok:
        Log::Info("DiscordRPC::ClearActivity", "Successfully clear activity");
        break;
    case DiscordRichPresence::Result::WritePipeFailed:
        // Pipe handle was most likely closed, try reconnecting
        Log::Error("DiscordRPC::ClearActivity", "Failed to clear activity (WritePipeFailed)");
        if (client->Reconnect() == DiscordRichPresence::Result::Ok) {
            Log::Info("DiscordRPC::ClearActivity", "Reconnected to IPC, retrying...");
            goto update_activity;
        } else {
            Log::Error("DiscordRPC::ClearActivity", "Failed to reconnect to IPC");
        }
        break;
    default:
        Log::Error("DiscordRPC::ClearActivity", "Failed to clear activity");
        break;
    }
}
