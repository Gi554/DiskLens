#include "core/Duplicates.h"
#include "core/Path.h"
#include "core/TreemapLayout.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    std::cout << (condition ? "PASS " : "FAIL ") << message << '\n';
    if (!condition) ++failures;
}
void write(const disklens::fs::path& path, const QByteArray& bytes) {
    disklens::fs::create_directories(path.parent_path());
    QFile file(disklens::displayPath(path));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        throw std::runtime_error("Could not create fixture " + path.u8string() + ": " +
                                 file.errorString().toStdString());
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace disklens;
    QTemporaryDir temporary(QDir::currentPath() + "/disklens-test-XXXXXX");
    if (!temporary.isValid()) return 2;
    const auto root = nativePath(temporary.path());
    write(root / "a.bin", QByteArray(4096, 'a'));
    write(root / "nested" / "copy.bin", QByteArray(4096, 'a'));
    write(root / "nested" / "different.bin", QByteArray(4096, 'b'));
    write(root / "empty", QByteArray());
    write(root / fs::u8path("données") / fs::u8path("été.txt"), QByteArray(17, 'c'));
    fs::create_directories(root / "empty-directory");
    std::atomic<bool> cancel{false};
    Progress progress;
    auto result = scan(root, cancel, progress);
    check(!result.cancelled && result.errors == 0, "scan completes");
    check(result.nodes[0].bytes == 12305, "root logical byte total");
    check(result.nodes[0].files == 5, "zero-length files are counted");
    check(result.directories == 4, "empty and Unicode directories are counted");
    check(progress.files == 5 && progress.bytes == 12305, "progress agrees with final totals");
    check(std::any_of(result.nodes.begin(), result.nodes.end(), [](const Node& node) {
        return displayPath(node.path.filename()) == QString::fromUtf8("été.txt");
    }), "Unicode paths round-trip");
    bool consistent = true;
    for (NodeId id = 1; id < result.nodes.size(); ++id) {
        const auto& parent = result.nodes[result.nodes[id].parent];
        consistent = consistent && std::find(parent.children.begin(), parent.children.end(), id) != parent.children.end();
    }
    check(consistent, "all child-parent relationships are valid");

    progress.reset();
    auto duplicates = findDuplicates(result, cancel, progress);
    check(duplicates.groups.size() == 1 && duplicates.groups[0].files.size() == 2,
          "same size alone does not imply duplicate content");
    check(duplicates.redundantBytes == 4096, "redundancy excludes one original copy");
    check(progress.candidates == 3 && progress.processed == 3, "hash only size candidates");
    write(root / "nested" / "copy.bin", QByteArray(5000, 'd'));
    progress.reset();
    duplicates = findDuplicates(result, cancel, progress);
    check(duplicates.groups.empty() && duplicates.changed == 1, "files changed after scan are excluded");

    std::error_code ec;
    fs::create_hard_link(root / "a.bin", root / "hard.bin", ec);
    if (!ec) {
        progress.reset(); result = scan(root, cancel, progress);
        progress.reset(); duplicates = findDuplicates(result, cancel, progress);
        check(duplicates.groups.empty() && duplicates.hardLinks == 1,
              "hard links are not redundant copies");
    } else std::cout << "SKIP hard link creation: " << ec.message() << '\n';
    ec.clear();
    fs::create_directory_symlink(root, root / "nested" / "loop", ec);
    if (!ec) {
        progress.reset(); result = scan(root, cancel, progress);
        check(result.skippedLinks == 1, "symbolic directory cycles are skipped");
    } else std::cout << "SKIP symlink creation: " << ec.message() << '\n';
    progress.reset(); cancel = true;
    result = scan(root, cancel, progress);
    check(result.cancelled && result.nodes[0].files == 0, "scan cancellation");
    duplicates = findDuplicates(result, cancel, progress);
    check(duplicates.cancelled, "duplicate search cancellation");
    cancel = false; progress.reset();
    result = scan(root / "does-not-exist", cancel, progress);
    check(result.nodes.empty() && result.errors == 1, "missing directory reported");
    result = scan(root / "a.bin", cancel, progress);
    check(result.nodes.empty() && result.errors == 1, "file cannot be scan root");

    const auto rectangles = layoutTreemap({{1, 100}, {2, 50}, {3, 25}, {4, 0}}, 700, 400);
    check(rectangles.size() == 3, "treemap omits zero-byte entries");
    double area = 0; bool geometry = true;
    for (const auto& rectangle : rectangles) {
        area += rectangle.width * rectangle.height;
        geometry = geometry && rectangle.x >= 0 && rectangle.y >= 0 &&
                   rectangle.x + rectangle.width <= 700.00001 &&
                   rectangle.y + rectangle.height <= 400.00001;
        const double expected = (rectangle.id == 1 ? 100 : rectangle.id == 2 ? 50 : 25) / 175.0;
        geometry = geometry && std::abs(rectangle.width * rectangle.height / 280000 - expected) < 1e-9;
    }
    check(geometry && std::abs(area - 280000) < 1e-5, "treemap areas proportional and bounded");
    bool disjoint = true;
    for (std::size_t i = 0; i < rectangles.size(); ++i) {
        for (std::size_t j = i + 1; j < rectangles.size(); ++j) {
            const auto& a = rectangles[i]; const auto& b = rectangles[j];
            const double overlapX = std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x);
            const double overlapY = std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y);
            if (overlapX > 1e-7 && overlapY > 1e-7) disjoint = false;
        }
    }
    check(disjoint, "treemap tiles never overlap");
    check(layoutTreemap({}, 100, 100).empty() && layoutTreemap({{1, 1}}, 0, 100).empty(),
          "treemap handles empty and invalid areas");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
