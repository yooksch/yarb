#pragma once

class GUI {
public:
    bool quit = false;

    GUI(int width, int height);
    ~GUI();
    virtual void Render() = 0;
    void Show();
};

class SettingsGUI : public GUI {
public:
    SettingsGUI(int width, int height) : GUI(width, height) {}
    void Render() override;
};

class LaunchGUI : public GUI {
public:
    enum Stage { VerifyingFileIntegrity, GettingLatestVersion, DownloadingManifest, DownloadingPackages } Stage;
    int progress_max = 0;
    int progress_current = 0;

    LaunchGUI(int width, int height) : GUI(width, height) {}
    void Render() override;
};
