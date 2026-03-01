#include "Illumina.hpp"

#include <stdexcept>
#include <cstdlib>

namespace NGS {

std::mt19937& Illumina::rng() {
    static std::mt19937 gen{ std::random_device{}() };
    return gen;
}

void Illumina::set_library(std::vector<std::string> lib) {
    library_ = std::move(lib);
    clusters_.clear();
    reads_.clear();
    cycle_ = 0;
    width_ = 0;
    height_ = 0;
}

void Illumina::set_read_length(int len) {
    if (len <= 0) throw std::invalid_argument("read_length must be > 0");
    read_len_ = len;
}

void Illumina::init(int width, int height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("width/height must be > 0");
    if (library_.empty()) throw std::invalid_argument("library is empty");
    if (read_len_ <= 0) throw std::invalid_argument("read_length not set");

    width_ = width;
    height_ = height;

    const bool reuse_positions = (clusters_.size() == library_.size());

    std::uniform_int_distribution<int> xd(10, width_ - 11);
    std::uniform_int_distribution<int> yd(10, height_ - 11);

    if (!reuse_positions) {
        clusters_.clear();
        clusters_.reserve(library_.size());
        for (const auto& s : library_) {
            Cluster c;
            c.pos = cv::Point(xd(rng()), yd(rng()));
            c.seq = s;
            c.index = 0;
            clusters_.push_back(std::move(c));
        }
    } else {

        for (size_t i = 0; i < library_.size(); ++i) {
            clusters_[i].seq = library_[i];
            clusters_[i].index = 0;
        }
    }

    reads_.assign(library_.size(), "");
    cycle_ = 0;
}

cv::Scalar Illumina::base_color_bgr(char b) {
    switch (b) {
    case 'A': return cv::Scalar(0, 0, 255);     // red
    case 'C': return cv::Scalar(255, 0, 0);     // blue
    case 'G': return cv::Scalar(0, 255, 0);     // green
    case 'T': return cv::Scalar(0, 255, 255);   // yellow
    default:  return cv::Scalar(0, 0, 0);
    }
}

char Illumina::decode_bgr(const cv::Vec3b& px) {
    const int b = px[0], g = px[1], r = px[2];
    if (r + g + b < 30) return 'N';

    struct Cand { char base; cv::Vec3b c; };
    static const Cand cand[4] = {
        {'A', cv::Vec3b(0, 0, 255)},
        {'C', cv::Vec3b(255, 0, 0)},
        {'G', cv::Vec3b(0, 255, 0)},
        {'T', cv::Vec3b(0, 255, 255)}
    };

    auto dist = [&](cv::Vec3b c) {
        return std::abs(b - c[0]) + std::abs(g - c[1]) + std::abs(r - c[2]);
    };

    char best = 'N';
    int bestd = 1'000'000;
    for (auto& x : cand) {
        int d = dist(x.c);
        if (d < bestd) { bestd = d; best = x.base; }
    }
    return best;
}

cv::Mat Illumina::step() {
    if (width_ <= 0 || height_ <= 0) throw std::runtime_error("call init(width,height) first");
    if (read_len_ <= 0) throw std::runtime_error("call set_read_length(len) first");


    if (cycle_ >= read_len_) {
        return cv::Mat(height_, width_, CV_8UC3, cv::Scalar(0, 0, 0));
    }

    cv::Mat frame(height_, width_, CV_8UC3, cv::Scalar(0, 0, 0));


    for (auto& c : clusters_) {
        if (c.index < c.seq.size()) {
            cv::circle(frame, c.pos, radius_, base_color_bgr(c.seq[c.index]), cv::FILLED, cv::LINE_AA);
        }
    }


    for (size_t i = 0; i < clusters_.size(); ++i) {
        auto& c = clusters_[i];

        const cv::Vec3b px = frame.at<cv::Vec3b>(c.pos.y, c.pos.x);
        const char called = decode_bgr(px);
        reads_[i].push_back(called);

        if (c.index < c.seq.size()) c.index++;
    }

    cycle_++;
    return frame;
}

}
