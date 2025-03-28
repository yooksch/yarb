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

class UpdateGUI : public GUI {
public:
    enum Stage { DownloadingManifest, DownloadingPackages } Stage;
    int package_count = 100;
    int packages_installed = 0;

    UpdateGUI(int width, int height) : GUI(width, height) {}
    void Render() override;
};
