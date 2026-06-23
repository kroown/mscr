#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "scanner.hpp"

ImageMeta parse_image_metadata(const std::vector<uint8_t>& data, const std::string& ext);

bool strip_image_metadata(const std::string& path, const std::string& ext);
