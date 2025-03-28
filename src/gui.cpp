#include "gui.hpp"
#include "game.hpp"
#include "log.hpp"
#include "embedded.hpp"
#include "config.hpp"
#include "paths.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <algorithm>
#include <combaseapi.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <objbase.h>
#include <shobjidl_core.h>
#include <string.h>
#include <thread>
#include <windows.h>
#include <shellapi.h>
#include <wingdi.h>
#include <winuser.h>
#include <ShObjIdl.h>
#include <gl/GL.h>

namespace fs = std::filesystem;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int window_width;
static int window_height;
static WNDCLASSEXW wc;
static HWND hWnd;
static HDC hdc;
static HGLRC hglrc;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            window_width = LOWORD(lParam);
            window_height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        // Disable alt menu
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool CreateDeviceWGL(HWND hWnd, HDC* hdc) {
    HDC dc = GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd;
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ChoosePixelFormat(dc, &pfd);
    if (pf == 0)
        return false;
    if (SetPixelFormat(dc, pf, &pfd) == FALSE)
        return false;
    ReleaseDC(hWnd, dc);

    *hdc = dc;
    if (!hglrc)
        hglrc = wglCreateContext(dc);
    return true;
}

GUI::GUI(int width, int height) {
    wc = {
        sizeof(wc),
        CS_OWNDC,
        WndProc,
        0,
        0,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"yarb",
        nullptr
    };
    RegisterClassExW(&wc);
    hWnd = CreateWindowW(
        wc.lpszClassName,
        L"Yet Another Roblox Bootstrapper",
        WS_OVERLAPPEDWINDOW,
        300,
        300,
        width,
        height,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    if (!CreateDeviceWGL(hWnd, &hdc)) {
        Log::Error("GUI::GUI", "Failed to create WGL device");
        exit(1);
    }

    wglMakeCurrent(hdc, hglrc);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    io.Fonts->AddFontFromMemoryCompressedTTF(INTER_MEDIUM_DATA, INTER_MEDIUM_SIZE, 16.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0;

    ImGui_ImplWin32_InitForOpenGL(hWnd);
    ImGui_ImplOpenGL3_Init();
}

GUI::~GUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    wglMakeCurrent(nullptr, nullptr);
    ReleaseDC(hWnd, hdc);

    wglDeleteContext(hglrc);
    DestroyWindow(hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    hWnd = nullptr;
    hdc = nullptr;
    hglrc = nullptr;
}

void GUI::Show() {
    while (!quit) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                quit = true;
        }
        if (quit)
            break;
        if (IsIconic(hWnd)) {
            Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        this->Render();

        ImGui::Render();
        glViewport(0, 0, window_width, window_height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(hdc);
    }
}

void HelpText(const char* text) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void SettingsGUI::Render() {
    ImGui::SetNextWindowSize({ (float)window_width, (float)window_height });
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::Begin("Main", nullptr,
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings);

    ImGui::BeginTabBar(".");

    auto config = Config::GetInstance();

    if (ImGui::BeginTabItem("Start")) {
        if (ImGui::CollapsingHeader("Control", ImGuiTreeNodeFlags_DefaultOpen)) {
            static bool safe_mode;
            static enum { GettingVersion, VerifyingFileIntegrity, DownloadingManifest, DownloadingPackages } start_stage;
            static int progress_max = 0;
            static int progress_current = 0;
            static bool started = false;
            if (ImGui::Button("Start Roblox")) {
                config->Save(Paths::ConfigFile);
                start_stage = GettingVersion;

                std::thread([=] {
                    auto version = Game::GetLatestRobloxVersion();
                    if (version == config->installed_version) {
                        Log::Info("SettingsGUI::Render", "Roblox is up to date");
                        if (config->verify_integrity_on_launch) {
                            start_stage = VerifyingFileIntegrity;
                            Game::VerifyFileIntegrity([&](int current, int total) {
                                progress_current = current;
                                progress_max = total;
                            });
                        }
                    } else {
                        start_stage = DownloadingManifest;
                        auto manifest = Game::GetManifest(version);
                        progress_current = 0;
                        progress_max = manifest.size();

                        start_stage = DownloadingPackages;
                        Game::Download(
                            version,
                            manifest,
                            config->efficient_download,
                            [&](int progress) {
                                progress_current = progress;
                            }
                        );
                    }

                    started = true;
                    Game::Start("--app");
                }).detach();
                ImGui::OpenPopup("Starting Roblox");
            }

            if (ImGui::BeginPopupModal("Starting Roblox", NULL,
                ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoSavedSettings
            )) {
                switch (start_stage) {
                case GettingVersion:
                    ImGui::Text("Stage: Getting latest version");
                    break;
                case VerifyingFileIntegrity:
                    ImGui::Text("Stage: Verifying file integrity");
                    if (progress_max == 0) {
                        ImGui::ProgressBar(0);
                    } else {
                        ImGui::ProgressBar((float)progress_current / progress_max);
                    }
                    break;
                case DownloadingManifest:
                    ImGui::Text("Stage: Downloading manifest");
                    break;
                case DownloadingPackages:
                    ImGui::Text("Stage: Downloading packages");
                    if (progress_max == 0) {
                        ImGui::ProgressBar(0);
                    } else {
                        ImGui::ProgressBar((float)progress_current / progress_max);
                    }
                    break;
                }

                if (started)
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::Checkbox("Safe Mode", &safe_mode);
            ImGui::SameLine();
            HelpText("Start Roblox without FastFlags and modifications");

            if (ImGui::Button("Reinstall Roblox")) {
                ImGui::OpenPopup("Installer");
            }

            if (ImGui::BeginPopupModal("Installer", NULL,
                ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoSavedSettings
            )) {
                enum State {
                    Uninstalling,
                    GettingVersion,
                    DownloadingManifest,
                    DownloadingPackages
                };

                static bool busy = false;
                static State state = GettingVersion;
                static int package_count = 100;
                static int packages_installed = 0;

                if (!busy) {
                    busy = true;

                    std::thread([] {
                        state = Uninstalling;
                        fs::remove_all(Paths::GameDirectory);
                        fs::create_directory(Paths::GameDirectory); // Ensure game directory exists
                        state = GettingVersion;
                        auto version = Game::GetLatestRobloxVersion();
                        state = DownloadingManifest;
                        auto manifest = Game::GetManifest(version);
                        state = DownloadingPackages;
                        package_count = manifest.size();

                        Game::Download(version, manifest, false, [](int p) {
                            packages_installed = p;
                        });

                        Sleep(500);
                        busy = false;
                    }).detach();
                }

                switch (state) {
                case Uninstalling:
                    ImGui::Text("State: Uninstalling Roblox");
                    break;
                case GettingVersion:
                    ImGui::Text("State: Getting version");
                    break;
                case DownloadingManifest:
                    ImGui::Text("State: Downloading manifest");
                    break;
                case DownloadingPackages:
                    ImGui::Text("State: Downloading packages");
                    ImGui::ProgressBar((float)packages_installed / package_count);
                    break;
                }

                if (package_count - 1 == packages_installed) {
                    packages_installed = 0;
                    state = Uninstalling;
                    ImGui::CloseCurrentPopup();
                }
            
                ImGui::EndPopup();
            }

            if (ImGui::Button("Open data folder")) {
                ShellExecuteA(NULL, "open", Paths::RootDirectory.string().data(), NULL, NULL, SW_SHOWDEFAULT);
            }

            if (ImGui::Button("Install YARB")) {
                
            }
            ImGui::SameLine();
            HelpText("Install YARB as a Windows application");
        }

        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Prevent multi launch", &config->prevent_multi_launch);
            ImGui::SameLine();
            HelpText("Prevents multiple Roblox instances starting");

            ImGui::Checkbox("Query server location", &config->query_server_location);
            ImGui::SameLine();
            HelpText("Query the Roblox server location");

            ImGui::Checkbox("Efficient downloads", &config->efficient_download);
            ImGui::SameLine();
            HelpText("Only download packages that changed. Control>Reinstall ignores this setting.");

            ImGui::Checkbox("Ensure file integrity", &config->verify_integrity_on_launch);
            ImGui::SameLine();
            HelpText("Verifies the integrity of every file at launch. May increase load times significantly");

            ImGui::Checkbox("Debug Mode", &config->debug_mode);
            ImGui::SameLine();
            HelpText("Start yarb in debug mode. Creates a console window");
        }

        if (ImGui::CollapsingHeader("Easy Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto easy_flags = &config->easy_flags;

            // Disable telemetry
            ImGui::Checkbox("Disable telemetry", &easy_flags->disable_telemetry);
            ImGui::SameLine();
            HelpText("Disable Roblox' client-sided telemtry");

            // Disable ads
            ImGui::Checkbox("Disable ingame ads", &easy_flags->disable_ads);
            ImGui::SameLine();
            HelpText("Disables the ingame ad service");

            // Render API
            const char* render_apis[] = { "Default", "D3D10", "D3D11", "Vulkan", "OpenGL" };
            if (ImGui::BeginCombo("Render API", easy_flags->render_api.data())) {
                for (size_t i = 0; i < IM_ARRAYSIZE(render_apis); i++) {
                    if (ImGui::Selectable(render_apis[i], easy_flags->render_api == render_apis[i]))
                        easy_flags->render_api = render_apis[i];
                    else
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            HelpText("\"Default\" lets Roblox pick the API");

            // FPS Limit
            if (ImGui::InputInt("FPS limit", &easy_flags->fps_limit)) {
                if (easy_flags->fps_limit < 0)
                    easy_flags->fps_limit = 0;
            }
            ImGui::SameLine();
            HelpText("0 = default");

            // Lighting Technology
            const char* lighting_technologies[] = { "Automatic", "Voxel", "ShadowMap", "Future" };
            if (ImGui::BeginCombo("Lighting technology", easy_flags->lighting_technology.data())) {
                for (size_t i = 0; i < IM_ARRAYSIZE(lighting_technologies); i++) {
                    if (ImGui::Selectable(lighting_technologies[i], easy_flags->lighting_technology == lighting_technologies[i]))
                        easy_flags->lighting_technology = lighting_technologies[i];
                    else
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            HelpText("Automatic respects the place's preferred lighting technology");

            // DPI scaling
            ImGui::Checkbox("DPI scaling", &easy_flags->dpi_scaling);
            ImGui::SameLine();
            HelpText("Disable to preserve rendering quality");

            // Shadows
            ImGui::Checkbox("Enable shadows", &easy_flags->shadows);
            
            // Render distance
            if (ImGui::InputInt("Render distance", &easy_flags->render_distance, 1)) {
                easy_flags->render_distance = std::clamp(easy_flags->render_distance, 0, 10);
            }
            ImGui::SameLine();
            HelpText("Changes the render distance. 1 = lowest, 10 = highest, 0 = default");

            // Limit light updates
            ImGui::Checkbox("Limit light updates", &easy_flags->limit_light_updates);
            ImGui::SameLine();
            HelpText("May slightly improve performance");

            // Light fades
            ImGui::Checkbox("Enable light fades", &easy_flags->light_fades);
            ImGui::SameLine();
            HelpText("Smoothes the transition between lights turning on or off");

            // Fix fog
            ImGui::Checkbox("Fix fog", &easy_flags->fix_fog);
            ImGui::SameLine();
            HelpText("Improves visibility within fog");

            // Post fx
            ImGui::Checkbox("Post FX", &easy_flags->post_fx);
            ImGui::SameLine();
            HelpText("Enables various post-processing effects");

            // Better vision
            ImGui::Checkbox("Better Vision", &easy_flags->better_vision);
            ImGui::SameLine();
            HelpText("Slightly improve vision in certain games");
        }

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("FastFlags")) {
        // FastFlagEditor inputs
        static char ffe_name[255];
        static char ffe_value[255];

        // Controls
        if (ImGui::Button("Add new FastFlag")) {
            ImGui::OpenPopup("FastFlagEditor");
        }

        bool open_editor = false;
        if (ImGui::BeginTable("fastflags", 3, ImGuiTableFlags_Borders)) {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.0f, 5.0f));

            // Setup header
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableHeadersRow();

            // Render fast flags
            size_t i = 0;
            for (const auto& flag : config->fast_flags) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", flag.first.c_str());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", flag.second.c_str());

                ImGui::TableNextColumn();
                ImGui::PushID(i++);
                if (ImGui::Button("Edit")) {
                    strcpy_s(ffe_name, flag.first.data());
                    strcpy_s(ffe_value, flag.second.data());
                    open_editor = true;
                }
                ImGui::PopID();

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.f, 0.f, 1.0f));

                ImGui::PushID(i++);
                if (ImGui::Button("Remove")) {
                    config->fast_flags.erase(flag.first);
                }
                ImGui::PopID();
                ImGui::PopStyleColor(3);
            }

            ImGui::PopStyleVar();
            ImGui::EndTable();
        }

        if (open_editor)
            ImGui::OpenPopup("FastFlagEditor");

        bool open = true;
        if (ImGui::BeginPopupModal("FastFlagEditor", &open,
        ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Name", ffe_name, sizeof(ffe_name));
            ImGui::InputText("Value", ffe_value, sizeof(ffe_value));

            if (ImGui::Button("Save")) {
                if (config->fast_flags.contains(ffe_name))
                    config->fast_flags.erase(ffe_name);
                config->fast_flags.emplace(ffe_name, ffe_value);
                memset(ffe_name, 0, sizeof(ffe_name));
                memset(ffe_value, 0, sizeof(ffe_value));
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                memset(ffe_name, 0, sizeof(ffe_name));
                memset(ffe_value, 0, sizeof(ffe_value));
                ImGui::CloseCurrentPopup();
            }
    
            ImGui::EndPopup();
        } else {
            memset(ffe_name, 0, sizeof(ffe_name));
            memset(ffe_value, 0, sizeof(ffe_value));
        }

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Modifications")) {
        if (ImGui::Button("Remove all modifications")) {
            ImGui::OpenPopup("Confirmation");
        }

        if (ImGui::BeginPopupModal("Confirmation", NULL, 
            ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Are you sure you want to remove all modifications?\nThis will delete all files in your Mods folder!");

            if (ImGui::Button("Confirm")) {
                fs::remove_all(Paths::ModsDirectory);
                fs::create_directories(Paths::ModsDirectory);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Abort")) 
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::SeparatorText("Game Files");

        int i = 0;
        auto create_tree = [&](this auto&& self, const fs::path& path) -> void {
            std::vector<fs::directory_entry> entries;

            // Populate entires
            for (const auto& entry : fs::directory_iterator(path))
                entries.emplace_back(entry);

            // Sort by type and then alphabetically (directories come first)
            std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                if (a.is_directory() != b.is_directory())
                    return a.is_directory();
                return a.path() < b.path();
            });

            for (const auto& entry : entries) {
                const auto& path = entry.path();
                if (entry.is_regular_file()) {
                    if (path.extension() == ".dll"
                        || path.filename() == "AppSettings.xml"
                        || path.extension() == ".exe"
                        || path.filename() == "COPYRIGHT.txt"
                        || path.filename() == "ClientAppSettings.json") continue;
                    
                    auto mod_path = Paths::ModsDirectory / fs::relative(path, Paths::GameDirectory);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%S", path.filename().c_str());
                    ImGui::SameLine();

                    ImGui::PushID(i++);
                    if (ImGui::Button("Overwrite")) {
                        HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                        if (FAILED(result)) {
                            Log::Error("SettingsGUI::Render", "Failed to open file dialog");
                            goto file_picker_failed;
                        }

                        IFileOpenDialog* dialog;
                        result = CoCreateInstance(
                            CLSID_FileOpenDialog,
                            NULL,
                            CLSCTX_ALL,
                            IID_IFileOpenDialog,
                            reinterpret_cast<void**>(&dialog)
                        );
                        if (FAILED(result)) {
                            Log::Error("SettingsGUI::Render", "Failed to open file dialog");
                            goto file_picker_failed;
                        }

                        result = dialog->Show(NULL);
                        if (FAILED(result)) goto file_picker_failed;

                        IShellItem* item;
                        result = dialog->GetResult(&item);
                        if (FAILED(result)) goto file_picker_failed;

                        PWSTR path;
                        result = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                        item->Release();
                        if (SUCCEEDED(result)) {
                            auto fs_path = fs::path(path);
                            fs::create_directories(mod_path.parent_path());
                            fs::copy_file(fs_path, mod_path);
                        } else {
                            goto file_picker_failed;
                        }

                        dialog->Release();
                        CoUninitialize();
                    }
                    file_picker_failed:
                    ImGui::PopID();

                    if (fs::exists(mod_path)) {
                        ImGui::SameLine();
                        ImGui::PushID(i++);
                        if (ImGui::Button("Restore")) {
                            fs::remove(mod_path);
                        }
                        ImGui::PopID();
                    }
                } else {
                    // Skip WebView2
                    if (path.filename().string().contains("WebView2")) continue;

                    ImGui::PushID(i++);
                    if (ImGui::TreeNode("##", "%S", path.filename().c_str())) {
                        self(path);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        };
        create_tree(Paths::GameDirectory);

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
}

void LaunchGUI::Render() {
    ImGui::SetNextWindowSize({ (float)window_width, (float)window_width });
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::Begin("Main", NULL,
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoSavedSettings);

    switch (Stage) {
    case VerifyingFileIntegrity:
        ImGui::Text("Stage: Verifying file integrity");
        if (progress_max == 0) {
            ImGui::ProgressBar(0);
        } else {
            ImGui::ProgressBar((float)progress_current / progress_max);
        }
        break;
    case GettingLatestVersion:
        ImGui::Text("Stage: Getting latest version");
        break;
    case DownloadingManifest:
        ImGui::Text("Stage: Downloading manifest");
        break;
    case DownloadingPackages:
        ImGui::Text("Stage: Downloading packages");
        ImGui::ProgressBar((float)progress_current / progress_max);
        break;
    }

    ImGui::End();
}