#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QScreen>
#include <QStyleFactory>
#include <QTimer>
#include <cstdio>
#include <algorithm>

namespace {
QIcon applicationIcon() {
    QPixmap image(128, 128); image.fill(Qt::transparent);
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen); painter.setBrush(QColor("#5069dd"));
    painter.drawRoundedRect(0, 0, 128, 128, 28, 28);
    painter.setBrush(Qt::white); painter.drawRoundedRect(24, 26, 36, 76, 5, 5);
    painter.setBrush(QColor("#aebcff")); painter.drawRoundedRect(68, 26, 36, 34, 5, 5);
    painter.setBrush(QColor("#76dcc5")); painter.drawRoundedRect(68, 68, 36, 34, 5, 5);
    return QIcon(image);
}

void applyTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI", 10));
    app.setStyleSheet(QString::fromUtf8(R"(
        QMainWindow { background: #f5f6fa; }
        QWidget { color: #24324a; }
        QLabel { background: transparent; }
        QLabel#brand { font-size: 27px; font-weight: 700; color: #1c2c4d; }
        QLabel#muted { color: #64738b; font-size: 13px; }
        QLabel#caption { color: #7b879b; font-size: 11px; }
        QLabel#badge { background: #e9eefc; color: #5069dd; padding: 7px 12px;
                      border-radius: 6px; font-size: 11px; font-weight: 600; }
        QLabel#metric { font-size: 25px; font-weight: 700; color: #24324a; }
        QLabel#section { color: #53627a; font-size: 12px; font-weight: 600; }
        QFrame#card { background: white; border: 1px solid #e2e7f0; border-radius: 10px; }
        QPushButton { background: white; border: 1px solid #d9e0ed; border-radius: 6px;
                      padding: 7px 12px; font-weight: 500; }
        QPushButton:hover { background: #edf0fb; border-color: #a9b7ee; }
        QPushButton:pressed { background: #dfe5fb; }
        QPushButton:disabled { color: #a6adbc; background: #f0f2f7; border-color: #e4e8f0; }
        QPushButton#primary { background: #5069dd; color: white; border-color: #5069dd; }
        QPushButton#primary:hover { background: #435bc9; }
        QPushButton#primary:disabled { background: #b2bce4; border-color: #b2bce4; }
        QLineEdit, QComboBox { background: white; border: 1px solid #dce2ee;
                              border-radius: 6px; padding: 8px; selection-background-color: #5069dd; }
        QLineEdit:focus, QComboBox:focus { border-color: #5069dd; }
        QComboBox::drop-down { border: none; width: 24px; }
        QComboBox QAbstractItemView { background: white; selection-background-color: #e4eafb; }
        QTreeWidget { background: white; alternate-background-color: #f7f9fc;
                      border: 1px solid #e2e7f0; border-radius: 6px; font-size: 12px; }
        QTreeWidget::item { padding: 9px 5px; border: none; }
        QTreeWidget::item:selected { background: #e5eafb; color: #24324a; }
        QHeaderView::section { background: #f0f3f9; border: none; padding: 11px 6px;
                              color: #62708a; font-weight: 600; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: #edf0f7; padding: 11px 10px; border-radius: 5px;
                      margin-right: 4px; color: #62708a; }
        QTabBar::tab:selected { background: #5069dd; color: white; }
        QSplitter::handle { background: transparent; width: 16px; }
        QProgressBar { background: #e7ecf6; border: none; border-radius: 4px;
                       text-align: center; color: #53627a; height: 14px; }
        QProgressBar::chunk { background: #5069dd; border-radius: 4px; }
        QToolTip { background: #24324a; color: white; padding: 8px; border: none; }
    )"));
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("DiskLens");
    app.setApplicationVersion("0.1.0");
    app.setWindowIcon(applicationIcon());
    applyTheme(app);
    QCommandLineParser parser;
    parser.setApplicationDescription("Explorateur visuel de l'espace disque.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("dossier", "Dossier à analyser au lancement.");
    parser.addOption({"smoke", "Analyser, rechercher les doublons, capturer puis quitter."});
    parser.addOption({"screenshot", "Chemin de la capture du test de rendu.", "fichier", "disklens.png"});
    parser.process(app);

    disklens::MainWindow window;
    if (auto* screen = app.primaryScreen()) {
        const auto available = screen->availableGeometry();
        window.resize(std::min(1200, available.width() - 60),
                      std::min(800, available.height() - 60));
    }
    window.show();
    const auto arguments = parser.positionalArguments();
    const bool smoke = parser.isSet("smoke");
    if (smoke && arguments.isEmpty()) {
        std::fprintf(stderr, "--smoke requires a folder\n");
        return 2;
    }
    if (smoke) {
        auto phase = std::make_shared<int>(0);
        window.onJobFinished = [&window, &app, phase, &parser] {
            if (!window.hasScan()) { app.exit(3); return; }
            if ((*phase)++ == 0) { window.runDuplicates(); return; }
            window.navigate(0);
            window.setSearch("report");
            window.setSearch("");
            QTimer::singleShot(200, &window, [&window, &app, &parser] {
                if (window.grab().save(parser.value("screenshot"))) app.exit(0);
                else app.exit(4);
            });
        };
        QTimer::singleShot(60000, &app, [&app] { app.exit(5); });
    }
    if (!arguments.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, arguments] { window.startScan(arguments.front()); });
    }
    return app.exec();
}
