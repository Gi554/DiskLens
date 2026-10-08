#include "core/Duplicates.h"
#include "core/Path.h"

#include <QCryptographicHash>
#include <QFile>
#include <algorithm>
#include <map>

namespace disklens {
namespace {
struct CountProcessed {
    Progress& progress;
    ~CountProcessed() { progress.processed.fetch_add(1, std::memory_order_relaxed); }
};
bool unchanged(const Node& node) {
    std::error_code ec;
    const auto status = fs::symlink_status(node.path, ec);
    if (ec || !fs::is_regular_file(status)) return false;
    const auto size = fs::file_size(node.path, ec);
    if (ec || size != node.bytes) return false;
    const auto modified = fs::last_write_time(node.path, ec);
    return !ec && modified == node.modified;
}
}

DuplicateResult findDuplicates(const ScanResult& scan, const std::atomic<bool>& cancel,
                               Progress& progress) {
    DuplicateResult result;
    std::map<std::uintmax_t, std::vector<NodeId>> sizes;
    for (NodeId id = 0; id < scan.nodes.size(); ++id) {
        if (cancel.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result;
        }
        const auto& node = scan.nodes[id];
        if (!node.directory && node.bytes > 0) sizes[node.bytes].push_back(id);
    }
    std::uint64_t candidates = 0;
    for (const auto& pair : sizes) {
        if (pair.second.size() > 1) candidates += pair.second.size();
    }
    progress.candidates = candidates;
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    for (const auto& pair : sizes) {
        if (pair.second.size() < 2) continue;
        std::map<QByteArray, std::vector<NodeId>> hashes;
        for (const NodeId id : pair.second) {
            if (cancel.load(std::memory_order_relaxed)) break;
            CountProcessed completed{progress};
            const auto& node = scan.nodes[id];
            if (!unchanged(node)) { ++result.changed; continue; }
            QFile file(displayPath(node.path));
            if (!file.open(QIODevice::ReadOnly)) { ++result.unreadable; continue; }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            std::uintmax_t read = 0;
            bool failed = false;
            while (!cancel.load(std::memory_order_relaxed)) {
                const auto amount = file.read(buffer.data(), buffer.size());
                if (amount < 0) { failed = true; break; }
                if (amount == 0) break;
                hash.addData(QByteArrayView(buffer.constData(), amount));
                read += static_cast<std::uintmax_t>(amount);
            }
            if (failed) { ++result.unreadable; continue; }
            if (cancel.load(std::memory_order_relaxed)) break;
            if (read != node.bytes || !unchanged(node)) { ++result.changed; continue; }
            auto& matches = hashes[hash.result()];
            bool hardLink = false;
            for (NodeId existing : matches) {
                std::error_code ec;
                if (fs::equivalent(node.path, scan.nodes[existing].path, ec) && !ec) {
                    hardLink = true;
                    break;
                }
            }
            if (hardLink) ++result.hardLinks;
            else matches.push_back(id);
        }
        for (auto& hash : hashes) {
            if (hash.second.size() > 1) {
                result.redundantBytes += pair.first * (hash.second.size() - 1);
                result.groups.push_back({std::move(hash.second), pair.first});
            }
        }
        if (cancel.load(std::memory_order_relaxed)) break;
    }
    result.cancelled = cancel.load(std::memory_order_relaxed);
    std::sort(result.groups.begin(), result.groups.end(), [](const auto& a, const auto& b) {
        return a.fileBytes * (a.files.size() - 1) > b.fileBytes * (b.files.size() - 1);
    });
    return result;
}
} // namespace disklens
