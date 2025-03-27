#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

class ModManager {
public:
    void ApplyModifications();
    void RestoreOriginalFiles();
    void ApplyFastFlags();
    void RemoveFastFlags();
    static ModManager* GetInstance();
private:
    std::unordered_map<std::filesystem::path, std::vector<char>> original_files;
};