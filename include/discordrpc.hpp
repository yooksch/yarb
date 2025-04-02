#pragma once

#include "game.hpp"

#include <memory>
#include <drpc/drpc.hpp>

class DiscordRPC {
public:
    void Init();
    void SetInGame(const Game::RobloxUniverseDetails place);
    void SetInApp();
    static DiscordRPC* GetInstance();
private:
    std::shared_ptr<DiscordRichPresence::Client> client;
    DiscordRichPresence::Activity activity;
    long long init_time;
};