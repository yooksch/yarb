#include "modmanager.hpp"
#include "config.hpp"
#include "log.hpp"
#include "paths.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

void ModManager::ApplyModifications() {
    auto apply_dir = [&, this](this auto&& self, const fs::path& path) -> void {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto rel_path = fs::relative(entry.path(), Paths::GameDirectory);
                auto mod_path = Paths::ModsDirectory / rel_path;
                if (fs::exists(mod_path) && fs::is_regular_file(mod_path)) {
                    // Create backup of original file
                    std::ifstream original_file(entry.path(), std::ios::binary);
                    std::vector<char> original_bytes {
                        std::istreambuf_iterator<char>(original_file),
                        std::istreambuf_iterator<char>()
                    };

                    // Store backup
                    original_files.emplace(entry.path(), original_bytes);

                    // Apply mod
                    fs::copy(mod_path, entry.path(), fs::copy_options::overwrite_existing);
                    Log::Debug("ModManager::ApplyModifications", "Applied modification for {}", rel_path.relative_path().string());
                }
            } else if (entry.is_directory()) {
                self(entry.path());
            }
        }
    };
    apply_dir(Paths::GameDirectory);
}

void ModManager::RestoreOriginalFiles() {
    for (const auto& original_file : original_files) {
        std::ofstream file(original_file.first, std::ios::binary);
        file.write(original_file.second.data(), original_file.second.size());
        
        Log::Debug("ModManager::RestoreOriginalFiles", "Restored file for {}", fs::relative(original_file.first, Paths::GameDirectory).string());
    }
    original_files.clear();
}

void ModManager::ApplyFastFlags() {
    auto config = Config::GetInstance();
    auto easy_flags = config->easy_flags;

    nlohmann::json flags;

    // Copy user-defined fastflags
    for (const auto& fflag : config->fast_flags) {
        flags[fflag.first] = fflag.second;
    }

    // Handle easy flags
    // Disable telemetry
    if (easy_flags.disable_telemetry) {
        flags["FFlagDebugDisableTelemetryEphemeralCounter"] = "True";
        flags["FFlagDebugDisableTelemetryEphemeralStat"] = "True";
        flags["FFlagDebugDisableTelemetryEventIngest"] = "True";
        flags["FFlagDebugDisableTelemetryPoint"] = "True";
        flags["FFlagDebugDisableTelemetryV2Counter"] = "True";
        flags["FFlagDebugDisableTelemetryV2Event"] = "True";
        flags["FFlagDebugDisableTelemetryV2Stat"] = "True";
    }

    // Render API
    {
        auto api = easy_flags.render_api;
        if (api == "Vulkan") {
            flags["FFlagDebugGraphicsDisableDirect3D11"] = "True";
            flags["FFlagDebugGraphicsPreferVulkan"] = "True";
        } else if (api == "OpenGL") {
            flags["FFlagDebugGraphicsDisableDirect3D11"] = "True";
            flags["FFlagDebugGraphicsPreferOpenGL"] = "True";            
        } else if (api == "D3D10") {
            flags["FFlagDebugGraphicsPreferD3D11FL10"] = "True";
        } else if (api == "D3D11") {
            flags["FFlagDebugGraphicsPreferD3D11"] = "True";
        }
    }

    // FPS limit
    if (easy_flags.fps_limit > 0) {
        flags["DFIntTaskSchedulerTargetFps"] = std::to_string(easy_flags.fps_limit);
        flags["FFlagTaskSchedulerLimitTargetFpsTo2402"] = "False";
    }

    // Lighting Technology
    {
        auto lt = easy_flags.lighting_technology;
        if (lt == "Voxel") {
            flags["DFFlagDebugRenderForceTechnologyVoxel"] = "True";
        } else if (lt == "ShadowMap") {
            flags["FFlagDebugForceFutureIsBrightPhase2"] = "True";
        } else if (lt == "Future") {
            flags["FFlagDebugForceFutureIsBrightPhase3"] = "True";
        }
    }

    // DPI scaling
    if (easy_flags.dpi_scaling) {
        flags["DFFlagDisableDPIScale"] = "False";
    } else {
        flags["DFFlagDisableDPIScale"] = "True";
    }

    // Shadows
    if (!easy_flags.shadows) {
        flags["FIntRenderShadowIntensity"] = "0";
    }

    // Render distance
    if (easy_flags.render_distance > 0) {
        flags["DFIntDebugRestrictGCDistance"] = std::to_string(easy_flags.render_distance);
    }

    // Limit light updates
    if (easy_flags.limit_light_updates) {
        flags["FIntRenderLocalLightUpdatesMax"] = "8";
        flags["FIntRenderLocalLightUpdatesMin"] = "6";
    }

    // Light fades
    if (!easy_flags.light_fades) {
        flags["FIntRenderLocalLightFadeInMs"] = "0";
    }

    // Fix fog
    if (easy_flags.fix_fog) {
        flags["FFlagRenderFixFog"] = "True";
    } else {
        flags["FFlagRenderFixFog"] = "False";
    }

    // Post FX
    if (easy_flags.post_fx) {
        flags["FFlagDisablePostFx"] = "False";
    } else {
        flags["FFlagDisablePostFx"] = "True";
    }

    // Better Vision
    if (easy_flags.better_vision) {
        flags["FFlagFastGPULightCulling3"] = "True";
        flags["FFlagNewLightAttenuation"] = "True";
    }

    // Disable ad service
    if (easy_flags.disable_ads) {
        flags["FFlagAdServiceEnabled"] = "False";
    }

    // Disable fullscreen titlebar
    if (easy_flags.disable_fullscreen_titlebar) {
        flags["FIntFullscreenTitleBarTriggerDelayMillis"] = "10000000";
    }

    // Write fast flags to Game/ClientSettings/ClientAppSettings.json
    auto cas_path = Paths::GameDirectory / "ClientSettings" / "ClientAppSettings.json";
    fs::create_directories(cas_path.parent_path());

    std::string s = flags.dump(4);
    std::ofstream file(cas_path);
    file.write(s.data(), s.size());
}

void ModManager::RemoveFastFlags() {
    auto cas_path = Paths::GameDirectory / "ClientSettings" / "ClientAppSettings.json";
    if (fs::exists(cas_path))
        fs::remove(cas_path);
}

ModManager* ModManager::GetInstance() {
    static ModManager instance;
    return &instance;
}