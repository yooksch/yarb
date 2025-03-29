#include "winhelpers.hpp"
#include "log.hpp"

#include <cstring>
#include <winnt.h>
#include <winreg.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/windows.ui.notifications.h>

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::Data::Xml::Dom;

constexpr const wchar_t* AUMID = L"com.yooksch.yarb";

void WinHelpers::RegisterProtocolHandler(const char *protocol, const std::filesystem::path &executable) {
    HKEY key {};
    LRESULT result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        std::format("Software\\Classes\\{}", protocol).c_str(),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &key,
        NULL
    );
    if (result != ERROR_SUCCESS) {
        Log::Error("Game::RegisterProtocolHandler", "Failed to register protocol handler");
        return;
    }

    std::string value = "URL: Roblox Protocol";
    RegSetValueExA(
        key,
        "",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        (value.length() + 1) * sizeof(wchar_t)
    );
    RegSetValueExA(
        key,
        "URL Protocol",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(""),
        1
    );
    RegCloseKey(key);

    result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        std::format("Software\\Classes\\{}\\DefaultIcon", protocol).c_str(),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &key,
        NULL
    );
    if (result != ERROR_SUCCESS) {
        Log::Error("Game::RegisterProtocolHandler", "Failed to register protocol handler");
        return;
    }

    value = executable.string();
    RegSetValueExA(
        key,
        "",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        (value.length() + 1) * sizeof(wchar_t)
    );
    RegCloseKey(key);

    result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        std::format("Software\\Classes\\{}\\shell\\open\\command", protocol).c_str(),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &key,
        NULL
    );
    if (result != ERROR_SUCCESS) {
        Log::Error("Game::RegisterProtocolHandler", "Failed to register protocol handler");
        return;
    }

    value = std::format("\"{}\" launch %1", executable.string());
    RegSetValueExA(
        key,
        "",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        (value.length() + 1) * sizeof(wchar_t)
    );
    RegCloseKey(key);
}

// Required for notifications
void WinHelpers::CreateUserAppModelIdEntry(const std::string& icon_uri) {
    HKEY key;
    LSTATUS result = RegCreateKeyW(HKEY_CURRENT_USER, std::format(L"Software\\Classes\\AppUserModelId\\{}", AUMID).c_str(), &key);
    if (result != ERROR_SUCCESS) {
        Log::Error("WinHelpers::CreateUserAppModelIdEntry", "Failed to create UserAppModelId entry");
        return;
    }

    std::wstring value = L"Yet Another Roblox Bootstrapper";
    result = RegSetValueExW(
        key,
        L"DisplayName",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        (value.length() + 1) * sizeof(wchar_t)
    );
    if (result != ERROR_SUCCESS) {
        Log::Error("WinHelpers::CreateUserAppModelIdEntry", "Failed to set display name");
        goto exit;
    }

    result = RegSetValueExA(
        key,
        "IconUri",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(icon_uri.c_str()),
        icon_uri.length() + 1
    );

    exit:
    RegCloseKey(key);
}

void WinHelpers::SendNotification(const std::wstring &title, const std::wstring &description) {
    XmlDocument doc;
    doc.LoadXml(L"<toast>\
        <visual>\
            <binding template=\"ToastGeneric\">\
                <text></text>\
                <text></text>\
            </binding>\
        </visual>\
    </toast>");

    // Populate with text and values
    doc.SelectSingleNode(L"//text[1]").InnerText(title.c_str());
    doc.SelectSingleNode(L"//text[2]").InnerText(description.c_str());

    // Construct the notification
    ToastNotification notif{ doc };

    ToastNotificationManager::CreateToastNotifier(AUMID).Show(notif);
}

std::wstring WinHelpers::StringToWideString(const std::string &str) {
    if (str.empty()) return std::wstring();

    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size);

    return wstrTo;
}