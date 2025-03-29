#pragma once

#include <filesystem>
#include <string>
#include <Windows.h>

namespace WinHelpers {
    void RegisterProtocolHandler(const char* protocol, const std::filesystem::path& executable);
    void CreateUserAppModelIdEntry(const std::string& icon_uri);
    void SendNotification(const std::wstring& title, const std::wstring& description);
    std::wstring StringToWideString(const std::string& str);
}