#include "Library.hpp"

#include <random>
#include <stdexcept>

namespace {

std::mt19937& rng() {
    static std::mt19937 gen{ std::random_device{}() };
    return gen;
}

size_t random_length(size_t min_len, size_t max_len) {
    std::uniform_int_distribution<size_t> dist(min_len, max_len);
    return dist(rng());
}

std::string gen_actg(size_t len) {
    static constexpr char bases[4] = { 'A','C','G','T' };
    std::uniform_int_distribution<int> dist(0, 3);

    std::string s(len, '\0');
    for (size_t i = 0; i < len; ++i) s[i] = bases[dist(rng())];
    return s;
}

void validate(const LibraryParameters& p) {
    if (p.total_reads == 0) throw std::invalid_argument("total_reads must be > 0");
    if (p.blocks.empty()) throw std::invalid_argument("blocks must not be empty");

    size_t sum = 0;
    for (const auto& b : p.blocks) {
        if (b.percent > 100) throw std::invalid_argument("percent must be <= 100");
        if (b.min_len == 0 || b.max_len == 0) throw std::invalid_argument("min_len/max_len must be > 0");
        if (b.min_len > b.max_len) throw std::invalid_argument("min_len > max_len");
        sum += b.percent;
    }
    if (sum > 100) throw std::invalid_argument("sum of percents must be <= 100");
}

}

std::vector<std::string> Library::generate(const LibraryParameters& p) const {
    validate(p);

    std::vector<std::string> out;
    out.reserve(p.total_reads);

    size_t used = 0;
    for (size_t i = 0; i < p.blocks.size(); ++i) {
        const auto& b = p.blocks[i];

        size_t take = (p.total_reads * b.percent) / 100;

        if (i + 1 == p.blocks.size()) take = p.total_reads - used;

        if (used + take > p.total_reads)
            throw std::runtime_error("percent allocation exceeds total_reads");

        used += take;

        for (size_t k = 0; k < take; ++k) {
            out.push_back(gen_actg(random_length(b.min_len, b.max_len)));
        }
    }

    return out;
}
