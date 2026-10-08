#pragma once

#include "core/Duplicates.h"
#include <QMainWindow>
#include <QElapsedTimer>
#include <functional>
#include <memory>
#include <thread>

class QLabel;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QTimer;
class QTreeWidget;
class QTabWidget;
class QComboBox;

namespace disklens {
class TreemapWidget;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void startScan(const QString& folder);
    bool busy() const { return job_ != Job::None; }
    bool hasScan() const { return scan_ && !scan_->nodes.empty(); }
    void runDuplicates();
    void navigate(NodeId id);
    void setSearch(const QString& text);
    std::function<void()> onJobFinished;

protected:
    void closeEvent(QCloseEvent*) override;

private:
    enum class Job { None, Scan, Duplicates };
    void buildUi();
    void refreshProgress();
    void finishJob();
    void updateTables();
    void showSelection(NodeId id);
    void openSelection();
    void exportCsv();
    void setBusy(bool busy);
    void populateDuplicates();
    void showWarnings();

    std::shared_ptr<ScanResult> scan_;
    std::shared_ptr<ScanResult> pendingScan_;
    DuplicateResult duplicates_;
    DuplicateResult pendingDuplicates_;
    QString failure_;
    NodeId directory_ = NoNode;
    NodeId selected_ = NoNode;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> finished_{false};
    Progress progress_;
    Job job_ = Job::None;
    QElapsedTimer elapsed_;

    TreemapWidget* map_ = nullptr;
    QLineEdit* folder_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* extension_ = nullptr;
    QPushButton* choose_ = nullptr;
    QPushButton* refresh_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* up_ = nullptr;
    QPushButton* duplicateButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* open_ = nullptr;
    QPushButton* issues_ = nullptr;
    QLabel* bytesLabel_ = nullptr;
    QLabel* filesLabel_ = nullptr;
    QLabel* dirsLabel_ = nullptr;
    QLabel* redundancyLabel_ = nullptr;
    QLabel* location_ = nullptr;
    QLabel* details_ = nullptr;
    QLabel* status_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTreeWidget* largest_ = nullptr;
    QTreeWidget* contents_ = nullptr;
    QTreeWidget* duplicateTree_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTimer* timer_ = nullptr;
};
} // namespace disklens
