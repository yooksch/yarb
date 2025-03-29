#pragma once

#include "game.hpp"
#include "discordgamesdk/include/core.h"
#include "discordgamesdk/include/types.h"

class DiscordRPC {
public:
    void Init();
    void SetActivity(const Game::RobloxUniverseDetails place);
    void ClearActivity();
    void StartThread();
    static DiscordRPC* GetInstance();
private:
    discord::Core* core;
    discord::Activity activity;
    bool running = false;
    long long init_time;
};