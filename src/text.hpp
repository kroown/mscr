#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "scanner.hpp"

TextMeta analyze_text(const std::vector<uint8_t>& data);
