#include "core/Path.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    std::cout << (condition ? "PASS " : "FAIL ") << message << '\n';
    if (!condition) ++failures;
}
bool waitForJob(disklens::MainWindow& window) {
    QElapsedTimer timeout; timeout.start();
    while (window.busy() && timeout.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    return !window.busy();
}
bool write(const QString& path, const QByteArray& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary(QDir::currentPath() + "/disklens-ui-XXXXXX");
    if (!temporary.isValid()) return 2;
    const auto root = temporary.path();
    if (!write(root + "/Alpha/report.txt", QByteArray(128, 'a')) ||
        !write(root + "/Beta/report-copy.txt", QByteArray(128, 'a')) ||
        !write(root + "/other.bin", QByteArray(256, 'b'))) return 2;

    disklens::MainWindow window;
    auto* contents = window.findChild<QTreeWidget*>("contents");
    auto* largest = window.findChild<QTreeWidget*>("largest");
    auto* duplicates = window.findChild<QTreeWidget*>("duplicates");
    auto* extension = window.findChild<QComboBox*>("extensions");
    auto* folder = window.findChild<QLineEdit*>("folder");
    auto* cancel = window.findChild<QPushButton*>("cancel");
    if (!contents || !largest || !duplicates || !extension || !folder || !cancel) return 2;
    window.startScan(root);
    check(window.busy(), "scan runs asynchronously");
    if (!waitForJob(window)) return 3;
    check(window.hasScan() && contents->topLevelItemCount() == 3 && largest->topLevelItemCount() == 3,
          "completed scan populates both lists");
    window.setSearch("report");
    check(largest->topLevelItemCount() == 2, "path search filters file list");
    window.setSearch("");
    extension->setCurrentIndex(extension->findData(".bin"));
    check(largest->topLevelItemCount() == 1, "extension filter selects binary file");
    extension->setCurrentIndex(0);
    auto* child = contents->topLevelItem(1);
    window.navigate(static_cast<disklens::NodeId>(child->data(0, Qt::UserRole).toULongLong()));
    check(contents->topLevelItemCount() == 1, "navigation opens a subdirectory");
    window.navigate(0);
    check(contents->topLevelItemCount() == 3, "navigation returns to root");
    window.runDuplicates();
    if (!waitForJob(window)) return 3;
    check(duplicates->topLevelItemCount() == 1 && duplicates->topLevelItem(0)->childCount() == 2,
          "duplicate search populates grouped paths");
    window.setSearch("does-not-exist");
    check(duplicates->topLevelItemCount() == 0, "search filters duplicate groups");
    window.setSearch("");
    check(duplicates->topLevelItemCount() == 1, "clearing search restores duplicate groups");

    const auto many = root + "/many";
    for (int i = 0; i < 2000; ++i)
        if (!write(many + "/" + QString::number(i) + ".txt", QByteArray(8, 'x'))) return 2;
    window.startScan(many);
    cancel->click();
    if (!waitForJob(window)) return 3;
    check(QDir::cleanPath(QDir::fromNativeSeparators(folder->text())) == QDir::cleanPath(root) &&
              largest->topLevelItemCount() == 3,
          "cancelled rescan retains previous result");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
