#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ParametersBlock {
    size_t percent = 0;
    size_t min_len = 0;
    size_t max_len = 0;
};

struct LibraryParameters {
    size_t total_reads = 0;
    std::vector<ParametersBlock> blocks;
};

class Library {
public:
    std::vector<std::string> generate(const LibraryParameters& p) const;
};
