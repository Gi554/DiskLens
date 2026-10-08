#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace disklens {
namespace fs = std::filesystem;
using NodeId = std::size_t;
constexpr NodeId NoNode = static_cast<NodeId>(-1);

struct Node {
    fs::path path;
    NodeId parent = NoNode;
    std::vector<NodeId> children;
    std::uintmax_t bytes = 0;
    std::uint64_t files = 0;
    bool directory = false;
    fs::file_time_type modified{};
};

struct Progress {
    std::atomic<std::uint64_t> files{0};
    std::atomic<std::uint64_t> directories{0};
    std::atomic<std::uintmax_t> bytes{0};
    std::atomic<std::uint64_t> processed{0};
    std::atomic<std::uint64_t> candidates{0};
    void reset();
};

struct ScanResult {
    std::vector<Node> nodes;
    std::uint64_t directories = 0;
    std::uint64_t skippedLinks = 0;
    std::uint64_t errors = 0;
    std::vector<std::string> warnings;
    bool cancelled = false;
};

ScanResult scan(const fs::path& root, const std::atomic<bool>& cancel, Progress& progress);
} // namespace disklens
