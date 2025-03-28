#include "config.hpp"
#include "log.hpp"
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Config::Config() {
    this->installed_version = "";
    this->verify_integrity_on_launch = false;
    this->query_server_location = false;
    this->debug_mode = false;
    this->prevent_multi_launch = true;
    this->efficient_download = true;
}

Config* Config::GetInstance() {
    static Config config;
    return &config;
}

void Config::Load(const std::filesystem::path& path) {
    try {
        std::ifstream stream(path);
        json j = json::parse(stream);

        this->installed_version = j["installed_version"];
        this->verify_integrity_on_launch = j["verify_integrity_on_launch"];
        this->prevent_multi_launch = j["prevent_multi_launch"];
        this->query_server_location = j["query_server_location"];
        this->debug_mode = j["debug_mode"];
        this->fast_flags = j["fast_flags"];
        this->efficient_download = j["efficient_download"];

        if (auto ef = j["easy_flags"]; ef.is_object()) {
            this->easy_flags = Config::EasyFlags {
                .disable_telemetry = ef["disable_telemetry"],
                .render_api = ef["render_api"],
                .fps_limit = ef["fps_limit"],
                .lighting_technology = ef["lighting_technology"],
                .dpi_scaling = ef["dpi_scaling"],
                .shadows = ef["shadows"],
                .render_distance = ef["render_distance"],
                .limit_light_updates = ef["limit_light_updates"],
                .light_fades = ef["light_fades"],
                .fix_fog = ef["fix_fog"],
                .post_fx = ef["post_fx"],
                .better_vision = ef["better_vision"],
                .disable_ads = ef["disable_ads"]
            };
        }
    } catch (std::exception ex) {
        Log::Error("Config::Load", "Failed to load config. Using defaults");
    }
}

void Config::Save(const std::filesystem::path& path) {
    std::ofstream stream(path);

    json j;
    j["installed_version"] = this->installed_version;
    j["verify_integrity_on_launch"] = this->verify_integrity_on_launch;
    j["prevent_multi_launch"] = this->prevent_multi_launch;
    j["query_server_location"] = this->query_server_location;
    j["debug_mode"] = this->debug_mode;
    j["fast_flags"] = this->fast_flags;
    j["efficient_download"] = this->efficient_download;

    auto qs = this->easy_flags;
    j["easy_flags"] = json {
        { "disable_telemetry", qs.disable_telemetry },
        { "render_api", qs.render_api },
        { "fps_limit", qs.fps_limit },
        { "lighting_technology", qs.lighting_technology },
        { "dpi_scaling", qs.dpi_scaling },
        { "shadows", qs.shadows },
        { "render_distance", qs.render_distance },
        { "limit_light_updates", qs.limit_light_updates },
        { "light_fades", qs.light_fades },
        { "fix_fog", qs.fix_fog },
        { "post_fx", qs.post_fx },
        { "better_vision", qs.better_vision },
        { "disable_ads", qs.disable_ads }
    };

    std::string s = j.dump(4);
    stream.write(s.data(), s.size());
}