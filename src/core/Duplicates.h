#pragma once

#include "core/Scanner.h"

namespace disklens {
struct DuplicateGroup {
    std::vector<NodeId> files;
    std::uintmax_t fileBytes = 0;
};
struct DuplicateResult {
    std::vector<DuplicateGroup> groups;
    std::uint64_t unreadable = 0;
    std::uint64_t changed = 0;
    std::uint64_t hardLinks = 0;
    std::uintmax_t redundantBytes = 0;
    bool cancelled = false;
};
DuplicateResult findDuplicates(const ScanResult& scan, const std::atomic<bool>& cancel,
                               Progress& progress);
} // namespace disklens
