#include "core/Scanner.h"

#include <algorithm>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace disklens {
namespace {
bool isLinkLike(const fs::path& path, const fs::file_status& status) {
    if (fs::is_symlink(status)) return true;
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    (void)path;
    return false;
#endif
}
}
void Progress::reset() {
    files = 0;
    directories = 0;
    bytes = 0;
    processed = 0;
    candidates = 0;
}

ScanResult scan(const fs::path& root, const std::atomic<bool>& cancel, Progress& progress) {
    ScanResult result;
    auto warn = [&](const fs::path& path, const std::error_code& ec) {
        ++result.errors;
        if (result.warnings.size() < 100) {
            result.warnings.push_back(path.u8string() + ": " + ec.message());
        }
    };
    std::error_code ec;
    auto absolute = fs::absolute(root, ec);
    if (ec) {
        warn(root, ec);
        return result;
    }
    const auto status = fs::symlink_status(absolute, ec);
    if (ec || !fs::is_directory(status) || isLinkLike(absolute, status)) {
        warn(absolute, ec ? ec : std::make_error_code(std::errc::not_a_directory));
        return result;
    }
    Node rootNode;
    rootNode.path = absolute.lexically_normal();
    rootNode.directory = true;
    result.nodes.push_back(std::move(rootNode));
    result.directories = 1;
    progress.directories = 1;

    std::vector<NodeId> pending{0};
    while (!pending.empty() && !cancel.load(std::memory_order_relaxed)) {
        const NodeId parent = pending.back();
        pending.pop_back();
        fs::directory_iterator iterator(result.nodes[parent].path, ec);
        if (ec) {
            warn(result.nodes[parent].path, ec);
            ec.clear();
            continue;
        }
        const fs::directory_iterator end;
        while (iterator != end && !cancel.load(std::memory_order_relaxed)) {
            const auto entry = *iterator;
            const auto entryStatus = entry.symlink_status(ec);
            if (ec) {
                warn(entry.path(), ec);
                ec.clear();
            } else if (isLinkLike(entry.path(), entryStatus)) {
                ++result.skippedLinks;
            } else if (fs::is_directory(entryStatus) || fs::is_regular_file(entryStatus)) {
                Node node;
                node.path = entry.path();
                node.parent = parent;
                node.directory = fs::is_directory(entryStatus);
                bool readable = true;
                if (!node.directory) {
                    node.bytes = entry.file_size(ec);
                    if (ec) {
                        warn(entry.path(), ec);
                        ec.clear();
                        readable = false;
                    } else {
                        node.modified = entry.last_write_time(ec);
                        if (ec) {
                            warn(entry.path(), ec);
                            ec.clear();
                            readable = false;
                        }
                        node.files = 1;
                    }
                }
                if (readable) {
                    const NodeId id = result.nodes.size();
                    result.nodes.push_back(std::move(node));
                    result.nodes[parent].children.push_back(id);
                    if (result.nodes[id].directory) {
                        pending.push_back(id);
                        ++result.directories;
                        progress.directories.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        progress.files.fetch_add(1, std::memory_order_relaxed);
                        progress.bytes.fetch_add(result.nodes[id].bytes, std::memory_order_relaxed);
                    }
                }
            }
            iterator.increment(ec);
            if (ec) {
                warn(result.nodes[parent].path, ec);
                ec.clear();
                break;
            }
        }
    }
    result.cancelled = cancel.load(std::memory_order_relaxed);
    // Every child is appended after its parent, so a reverse pass aggregates the tree.
    for (NodeId id = result.nodes.size(); id-- > 1;) {
        const auto parent = result.nodes[id].parent;
        result.nodes[parent].bytes += result.nodes[id].bytes;
        result.nodes[parent].files += result.nodes[id].files;
    }
    for (auto& node : result.nodes) {
        std::sort(node.children.begin(), node.children.end(), [&](NodeId a, NodeId b) {
            if (result.nodes[a].bytes != result.nodes[b].bytes) {
                return result.nodes[a].bytes > result.nodes[b].bytes;
            }
            return result.nodes[a].path.native() < result.nodes[b].path.native();
        });
    }
    return result;
}
} // namespace disklens
