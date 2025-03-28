#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class Config {
public:
    struct EasyFlags {
        bool disable_telemetry = true;
        std::string render_api = "Default";
        int fps_limit = 0;
        std::string lighting_technology = "Automatic";
        bool dpi_scaling = true;
        bool shadows = true;
        int render_distance = 0;
        bool limit_light_updates = false;
        bool light_fades = true;
        bool fix_fog = false;
        bool post_fx = true;
        bool better_vision = false;
        bool disable_ads = true;
        bool disable_fullscreen_titlebar = false;
    };

    std::string installed_version;
    std::unordered_map<std::string, std::string> fast_flags;
    bool verify_integrity_on_launch;
    EasyFlags easy_flags;
    bool prevent_multi_launch;
    bool debug_mode;
    bool query_server_location;
    bool efficient_download;

    void Save(const std::filesystem::path& path);
    void Load(const std::filesystem::path& path);

    static Config* GetInstance();
private:
    Config();
};