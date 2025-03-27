#pragma once

class GUI {
public:
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

class LoadingScreenGUI : public GUI {
public:
    LoadingScreenGUI(int width, int height) : GUI(width, height) {}
    void Render() override;
};
