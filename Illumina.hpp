#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <random>

namespace NGS {

class Illumina final {
public:
    void set_library(std::vector<std::string> lib);
    void set_read_length(int len);
    void init(int width, int height);

    cv::Mat step();

    int cycle() const { return cycle_; }
    const std::vector<std::string>& reads() const { return reads_; }

private:
    struct Cluster {
        cv::Point pos{};
        std::string seq;
        size_t index = 0;
    };

    int width_ = 0;
    int height_ = 0;
    int radius_ = 1;
    int cycle_ = 0;
    int read_len_ = 0;

    std::vector<std::string> library_;
    std::vector<Cluster> clusters_;
    std::vector<std::string> reads_;

    static std::mt19937& rng();
    static cv::Scalar base_color_bgr(char b);
    static char decode_bgr(const cv::Vec3b& px);
};

}
