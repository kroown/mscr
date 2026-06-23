#pragma once
#include <string>
#include <vector>
#include "scanner.hpp"

std::string to_json(const std::vector<FileMetadata>& results, bool pretty);
