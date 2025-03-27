#pragma once

#include <string>
#include <vector>

namespace Crypto {
    std::string ComputeSHA256(const std::vector<char>& bytes);
}