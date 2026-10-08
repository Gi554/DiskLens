#include "ui/MainWindow.h"
#include "core/Path.h"
#include "ui/TreemapWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QStringConverter>
#include <QSplitter>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <algorithm>
#include <set>

namespace disklens {
namespace {
class SizeItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override {
        const int column = treeWidget()->sortColumn();
        if (column == 1) return data(1, Qt::UserRole).toULongLong() <
                                other.data(1, Qt::UserRole).toULongLong();
        return text(column).localeAwareCompare(other.text(column)) < 0;
    }
};
QTreeWidget* makeTree(const QStringList& labels) {
    auto* tree = new QTreeWidget;
    tree->setHeaderLabels(labels);
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setUniformRowHeights(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < labels.size(); ++i)
        tree->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    tree->setMinimumWidth(320);
    return tree;
}
QString csvCell(QString value) { value.replace('"', "\"\""); return '"' + value + '"'; }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("DiskLens");
    resize(1200, 800);
    setMinimumSize(980, 640);
    buildUi();
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this] { refreshProgress(); });
    timer_->start(100);
}

MainWindow::~MainWindow() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);
    setCentralWidget(central);

    auto* header = new QHBoxLayout;
    auto* brand = new QLabel("◈  DiskLens");
    brand->setObjectName("brand");
    auto* tagline = new QLabel("Votre espace disque, en clair.");
    tagline->setObjectName("muted");
    auto* identity = new QVBoxLayout;
    identity->setSpacing(4); identity->addWidget(brand); identity->addWidget(tagline);
    header->addLayout(identity); header->addStretch();
    auto* badge = new QLabel("LOCAL · LECTURE SEULE");
    badge->setObjectName("badge"); header->addWidget(badge);
    layout->addLayout(header);

    auto* toolbar = new QHBoxLayout;
    folder_ = new QLineEdit;
    folder_->setObjectName("folder");
    folder_->setReadOnly(true);
    folder_->setPlaceholderText("Sélectionnez un dossier à analyser…");
    folder_->setAccessibleName("Dossier analysé");
    choose_ = new QPushButton("Choisir un dossier"); choose_->setObjectName("primary");
    refresh_ = new QPushButton("Actualiser");
    cancelButton_ = new QPushButton("Annuler"); cancelButton_->setEnabled(false);
    cancelButton_->setObjectName("cancel");
    toolbar->addWidget(folder_, 1); toolbar->addWidget(choose_); toolbar->addWidget(refresh_);
    toolbar->addWidget(cancelButton_); layout->addLayout(toolbar);
    connect(choose_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, "Analyser un dossier", folder_->text());
        if (!path.isEmpty()) startScan(path);
    });
    connect(refresh_, &QPushButton::clicked, this, [this] { startScan(folder_->text()); });
    connect(cancelButton_, &QPushButton::clicked, this, [this] {
        cancel_ = true; cancelButton_->setEnabled(false); status_->setText("Annulation en cours…");
    });

    auto* cards = new QHBoxLayout;
    auto card = [&](const QString& title, QLabel*& value, const QString& note) {
        auto* frame = new QFrame; frame->setObjectName("card");
        auto* stack = new QVBoxLayout(frame); stack->setContentsMargins(14, 10, 14, 10);
        auto* label = new QLabel(title); label->setObjectName("muted");
        value = new QLabel("—"); value->setObjectName("metric");
        auto* caption = new QLabel(note); caption->setObjectName("caption");
        stack->addWidget(label); stack->addWidget(value); stack->addWidget(caption); cards->addWidget(frame, 1);
    };
    card("TAILLE TOTALE", bytesLabel_, "Taille logique des fichiers");
    card("FICHIERS", filesLabel_, "Tous les fichiers réguliers");
    card("DOSSIERS", dirsLabel_, "Dossier racine inclus");
    card("COPIES SUPPLÉMENTAIRES", redundancyLabel_, "Après recherche des doublons");
    layout->addLayout(cards);

    auto* filters = new QHBoxLayout;
    search_ = new QLineEdit; search_->setPlaceholderText("Filtrer par nom ou chemin…");
    search_->setObjectName("search");
    search_->setClearButtonEnabled(true); search_->setAccessibleName("Recherche de fichiers");
    extension_ = new QComboBox; extension_->addItem("Toutes les extensions", "");
    extension_->setObjectName("extensions");
    extension_->setMinimumWidth(175);
    duplicateButton_ = new QPushButton("Rechercher les doublons");
    exportButton_ = new QPushButton("Exporter CSV");
    filters->addWidget(search_, 1); filters->addWidget(extension_); filters->addWidget(duplicateButton_);
    filters->addWidget(exportButton_); layout->addLayout(filters);
    connect(search_, &QLineEdit::textChanged, this, [this] { updateTables(); map_->setFilter(search_->text()); });
    connect(extension_, &QComboBox::currentIndexChanged, this, [this] { updateTables(); });
    connect(duplicateButton_, &QPushButton::clicked, this, [this] { runDuplicates(); });
    connect(exportButton_, &QPushButton::clicked, this, [this] { exportCsv(); });

    auto* split = new QSplitter(Qt::Horizontal);
    auto* mapPanel = new QFrame; mapPanel->setObjectName("card");
    auto* mapLayout = new QVBoxLayout(mapPanel); mapLayout->setContentsMargins(14, 14, 14, 14);
    auto* mapHeader = new QHBoxLayout;
    up_ = new QPushButton("↑ Parent"); up_->setMaximumWidth(105);
    location_ = new QLabel("CARTE DE L'ESPACE"); location_->setObjectName("section");
    location_->setMinimumWidth(0);
    mapHeader->addWidget(up_); mapHeader->addWidget(location_, 1); mapLayout->addLayout(mapHeader);
    map_ = new TreemapWidget; mapLayout->addWidget(map_, 1);
    auto* legend = new QLabel("● Dossiers   ·   Images   ·   Médias   ·   Archives   ·   Code   ·   Autres");
    legend->setObjectName("caption"); legend->setWordWrap(true); mapLayout->addWidget(legend);
    auto* hint = new QLabel("Un rectangle = un élément du dossier. Double-cliquez pour explorer.");
    hint->setObjectName("caption"); hint->setWordWrap(true); mapLayout->addWidget(hint);
    split->addWidget(mapPanel);
    tabs_ = new QTabWidget;
    contents_ = makeTree({"Élément", "Taille"});
    contents_->setObjectName("contents");
    largest_ = makeTree({"Fichier", "Taille"});
    largest_->setObjectName("largest");
    duplicateTree_ = makeTree({"Groupe / fichier", "Taille"});
    duplicateTree_->setObjectName("duplicates");
    duplicateTree_->setRootIsDecorated(true);
    tabs_->addTab(contents_, "Contenu"); tabs_->addTab(largest_, "Gros fichiers");
    tabs_->addTab(duplicateTree_, "Doublons"); split->addWidget(tabs_);
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 2);
    split->setSizes({760, 520}); layout->addWidget(split, 1);
    connect(up_, &QPushButton::clicked, this, [this] {
        if (scan_ && directory_ < scan_->nodes.size()) {
            const auto parent = scan_->nodes[directory_].parent;
            if (parent != NoNode) navigate(parent);
        }
    });
    map_->onSelected = [this](NodeId id) { showSelection(id); };
    map_->onActivated = [this](NodeId id) {
        if (scan_->nodes[id].directory) navigate(id);
        else { showSelection(id); openSelection(); }
    };
    for (auto* tree : {contents_, largest_, duplicateTree_}) {
        connect(tree, &QTreeWidget::itemSelectionChanged, this, [this, tree] {
            const auto items = tree->selectedItems();
            if (!items.isEmpty() && items.front()->data(0, Qt::UserRole).isValid())
                showSelection(static_cast<NodeId>(items.front()->data(0, Qt::UserRole).toULongLong()));
        });
        connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
            if (!item->data(0, Qt::UserRole).isValid() || !scan_) return;
            const auto id = static_cast<NodeId>(item->data(0, Qt::UserRole).toULongLong());
            if (scan_->nodes[id].directory) navigate(id);
            else { showSelection(id); openSelection(); }
        });
    }

    auto* selectionRow = new QHBoxLayout;
    details_ = new QLabel("Aucun élément sélectionné"); details_->setObjectName("muted");
    details_->setMinimumWidth(0); details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    open_ = new QPushButton("Ouvrir l'emplacement");
    selectionRow->addWidget(details_, 1); selectionRow->addWidget(open_); layout->addLayout(selectionRow);
    connect(open_, &QPushButton::clicked, this, [this] { openSelection(); });

    auto* footer = new QHBoxLayout;
    status_ = new QLabel("Prêt. Aucun fichier ne sera modifié par DiskLens.");
    status_->setObjectName("caption"); status_->setWordWrap(true);
    progressBar_ = new QProgressBar; progressBar_->setFixedWidth(180); progressBar_->setVisible(false);
    issues_ = new QPushButton("Détails"); issues_->setVisible(false);
    footer->addWidget(status_, 1); footer->addWidget(progressBar_); footer->addWidget(issues_);
    layout->addLayout(footer);
    connect(issues_, &QPushButton::clicked, this, [this] { showWarnings(); });

    auto* chooseShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(chooseShortcut, &QShortcut::activated, choose_, &QPushButton::click);
    auto* refreshShortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
    connect(refreshShortcut, &QShortcut::activated, refresh_, &QPushButton::click);
    auto* cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(cancelShortcut, &QShortcut::activated, cancelButton_, &QPushButton::click);
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, search_, qOverload<>(&QLineEdit::setFocus));
    setBusy(false);
}

void MainWindow::setBusy(bool busyNow) {
    choose_->setEnabled(!busyNow);
    refresh_->setEnabled(!busyNow && !folder_->text().isEmpty());
    cancelButton_->setEnabled(busyNow);
    duplicateButton_->setEnabled(!busyNow && hasScan());
    exportButton_->setEnabled(!busyNow && hasScan());
    up_->setEnabled(scan_ && directory_ != NoNode && directory_ != 0);
    open_->setEnabled(selected_ != NoNode);
    progressBar_->setVisible(busyNow);
    if (busyNow) progressBar_->setRange(0, 0);
}

void MainWindow::startScan(const QString& folder) {
    if (busy() || folder.isEmpty()) return;
    cancel_ = false; finished_ = false; progress_.reset(); failure_.clear();
    pendingScan_.reset(); job_ = Job::Scan; elapsed_.start();
    setBusy(true); status_->setText("Analyse en cours…");
    const auto path = nativePath(folder);
    worker_ = std::thread([this, path] {
        try { pendingScan_ = std::make_shared<ScanResult>(scan(path, cancel_, progress_)); }
        catch (const std::exception& error) { failure_ = QString::fromUtf8(error.what()); }
        catch (...) { failure_ = "Échec inattendu de l'analyse."; }
        finished_.store(true, std::memory_order_release);
    });
}

void MainWindow::runDuplicates() {
    if (busy() || !hasScan()) return;
    cancel_ = false; finished_ = false; progress_.reset(); failure_.clear();
    pendingDuplicates_ = {}; job_ = Job::Duplicates; elapsed_.start(); setBusy(true);
    status_->setText("Recherche des tailles identiques, puis comparaison SHA-256…");
    const auto snapshot = scan_;
    worker_ = std::thread([this, snapshot] {
        try { pendingDuplicates_ = findDuplicates(*snapshot, cancel_, progress_); }
        catch (const std::exception& error) { failure_ = QString::fromUtf8(error.what()); }
        catch (...) { failure_ = "Échec inattendu de la recherche de doublons."; }
        finished_.store(true, std::memory_order_release);
    });
}

void MainWindow::refreshProgress() {
    if (!busy()) return;
    if (finished_.load(std::memory_order_acquire)) { finishJob(); return; }
    if (cancel_.load()) return;
    if (job_ == Job::Scan) {
        status_->setText(QString("Analyse · %1 fichiers · %2 dossiers · %3 · %4 s")
                            .arg(progress_.files.load()).arg(progress_.directories.load())
                            .arg(formatBytes(progress_.bytes.load())).arg(elapsed_.elapsed() / 1000));
    } else {
        const auto total = progress_.candidates.load();
        const auto done = progress_.processed.load();
        if (total) {
            progressBar_->setRange(0, 100);
            progressBar_->setValue(static_cast<int>(100.0 * static_cast<double>(done) / total));
        }
        status_->setText(QString("Doublons · %1 / %2 candidats · %3 s")
                            .arg(done).arg(total).arg(elapsed_.elapsed() / 1000));
    }
}

void MainWindow::finishJob() {
    if (worker_.joinable()) worker_.join();
    const auto completed = job_; job_ = Job::None;
    if (!failure_.isEmpty()) {
        status_->setText("Échec : " + failure_);
    } else if (completed == Job::Scan) {
        if (pendingScan_ && !pendingScan_->cancelled && !pendingScan_->nodes.empty()) {
            scan_ = std::move(pendingScan_); duplicates_ = {}; directory_ = 0; selected_ = NoNode;
            folder_->setText(displayPath(scan_->nodes[0].path));
            bytesLabel_->setText(formatBytes(scan_->nodes[0].bytes));
            filesLabel_->setText(QString::number(scan_->nodes[0].files));
            dirsLabel_->setText(QString::number(scan_->directories)); redundancyLabel_->setText("—");
            std::set<QString> extensions;
            for (const auto& node : scan_->nodes)
                if (!node.directory) extensions.insert(displayPath(node.path.extension()).toLower());
            extension_->blockSignals(true); extension_->clear(); extension_->addItem("Toutes les extensions", "");
            for (const auto& ext : extensions)
                extension_->addItem(ext.isEmpty() ? "Sans extension" : ext, ext.isEmpty() ? "<none>" : ext);
            extension_->blockSignals(false);
            duplicateTree_->clear(); details_->setText("Aucun élément sélectionné");
            navigate(0);
            status_->setText(QString("Analyse terminée en %1 s · %2 accès ignorés · %3 liens ignorés")
                                .arg(elapsed_.elapsed() / 1000.0, 0, 'f', 1)
                                .arg(scan_->errors).arg(scan_->skippedLinks));
            issues_->setVisible(scan_->errors > 0 || scan_->skippedLinks > 0);
        } else if (pendingScan_ && pendingScan_->cancelled) {
            status_->setText("Analyse annulée. Le résultat précédent est conservé.");
        } else {
            status_->setText("Impossible d'analyser ce dossier. Le résultat précédent est conservé.");
            if (pendingScan_ && !pendingScan_->warnings.empty()) {
                QMessageBox::warning(this, "Dossier inaccessible",
                                      QString::fromUtf8(pendingScan_->warnings.front().c_str()));
            }
        }
    } else {
        if (pendingDuplicates_.cancelled) {
            status_->setText("Recherche annulée. Les résultats partiels ne sont pas affichés.");
        } else {
            duplicates_ = std::move(pendingDuplicates_);
            redundancyLabel_->setText(formatBytes(duplicates_.redundantBytes));
            populateDuplicates(); tabs_->setCurrentWidget(duplicateTree_);
            status_->setText(QString("%1 groupes · %2 fichiers illisibles · %3 modifiés · %4 liens physiques exclus")
                                .arg(duplicates_.groups.size()).arg(duplicates_.unreadable)
                                .arg(duplicates_.changed).arg(duplicates_.hardLinks));
        }
    }
    setBusy(false);
    if (onJobFinished) onJobFinished();
}

void MainWindow::navigate(NodeId id) {
    if (!scan_ || id >= scan_->nodes.size() || !scan_->nodes[id].directory) return;
    directory_ = id; selected_ = NoNode;
    map_->setDirectory(scan_, id); map_->setFilter(search_->text());
    const auto path = displayPath(scan_->nodes[id].path);
    location_->setText(location_->fontMetrics().elidedText(path, Qt::ElideMiddle, 480));
    location_->setToolTip(path); details_->setText("Aucun élément sélectionné");
    up_->setEnabled(id != 0); open_->setEnabled(false);
    updateTables();
}

void MainWindow::setSearch(const QString& text) { search_->setText(text); }

void MainWindow::updateTables() {
    if (!hasScan()) return;
    auto matches = [&](const Node& node) {
        if (!displayPath(node.path).contains(search_->text(), Qt::CaseInsensitive)) return false;
        const auto filter = extension_->currentData().toString();
        const auto ext = displayPath(node.path.extension()).toLower();
        return node.directory || filter.isEmpty() || (filter == "<none>" ? ext.isEmpty() : ext == filter);
    };
    auto add = [&](QTreeWidget* tree, NodeId id, bool fullPath) {
        const auto& node = scan_->nodes[id];
        auto* item = new SizeItem(tree);
        item->setText(0, fullPath ? displayPath(node.path.lexically_relative(scan_->nodes[0].path))
                                  : displayPath(node.path.filename()));
        item->setText(1, formatBytes(node.bytes));
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(id));
        item->setData(1, Qt::UserRole, QVariant::fromValue<qulonglong>(node.bytes));
        item->setToolTip(0, displayPath(node.path));
        item->setForeground(0, nodeColor(node));
        if (node.directory) { auto font = item->font(0); font.setBold(true); item->setFont(0, font); }
    };
    contents_->setSortingEnabled(false); contents_->clear();
    std::size_t shown = 0, total = 0;
    if (directory_ < scan_->nodes.size()) {
        for (auto id : scan_->nodes[directory_].children) {
            if (!matches(scan_->nodes[id])) continue;
            ++total; if (shown < 2000) { add(contents_, id, false); ++shown; }
        }
    }
    tabs_->setTabText(0, QString("Contenu %1/%2").arg(shown).arg(total));
    contents_->setSortingEnabled(true); contents_->sortItems(1, Qt::DescendingOrder);
    std::vector<NodeId> files;
    for (NodeId id = 0; id < scan_->nodes.size(); ++id)
        if (!scan_->nodes[id].directory && matches(scan_->nodes[id])) files.push_back(id);
    const auto count = std::min<std::size_t>(files.size(), 500);
    std::partial_sort(files.begin(), files.begin() + static_cast<std::ptrdiff_t>(count), files.end(),
                      [&](NodeId a, NodeId b) { return scan_->nodes[a].bytes > scan_->nodes[b].bytes; });
    largest_->setSortingEnabled(false); largest_->clear();
    for (std::size_t i = 0; i < count; ++i) add(largest_, files[i], true);
    largest_->setSortingEnabled(true); largest_->sortItems(1, Qt::DescendingOrder);
    tabs_->setTabText(1, QString("Gros fichiers %1").arg(count));
    populateDuplicates();
}

void MainWindow::populateDuplicates() {
    duplicateTree_->clear();
    if (!scan_) return;
    std::size_t shown = 0;
    for (const auto& group : duplicates_.groups) {
        bool matches = search_->text().isEmpty();
        const auto extFilter = extension_->currentData().toString();
        bool extensionMatches = extFilter.isEmpty();
        for (auto id : group.files) {
            const auto& node = scan_->nodes[id];
            matches = matches || displayPath(node.path).contains(search_->text(), Qt::CaseInsensitive);
            const auto ext = displayPath(node.path.extension()).toLower();
            extensionMatches = extensionMatches || (extFilter == "<none>" ? ext.isEmpty() : ext == extFilter);
        }
        if (!matches || !extensionMatches) continue;
        if (++shown > 500) break;
        auto* parent = new QTreeWidgetItem(duplicateTree_);
        parent->setText(0, QString("%1 copies · %2 supplémentaires")
                              .arg(group.files.size()).arg(formatBytes(group.fileBytes * (group.files.size() - 1))));
        parent->setText(1, formatBytes(group.fileBytes));
        for (auto id : group.files) {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, displayPath(scan_->nodes[id].path.lexically_relative(scan_->nodes[0].path)));
            item->setText(1, formatBytes(group.fileBytes));
            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(id));
            item->setToolTip(0, displayPath(scan_->nodes[id].path));
        }
        parent->setExpanded(true);
    }
}

void MainWindow::showSelection(NodeId id) {
    if (!scan_ || id >= scan_->nodes.size()) return;
    selected_ = id; map_->setSelected(id);
    const auto& node = scan_->nodes[id];
    const auto full = displayPath(node.path) + " · " + formatBytes(node.bytes);
    details_->setText(details_->fontMetrics().elidedText(full, Qt::ElideMiddle, width() - 300));
    details_->setToolTip(full); open_->setEnabled(true);
}

void MainWindow::openSelection() {
    if (!scan_ || selected_ >= scan_->nodes.size()) return;
    const auto& node = scan_->nodes[selected_];
    const auto path = node.directory ? node.path : node.path.parent_path();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(displayPath(path))))
        QMessageBox::warning(this, "Ouverture impossible", "Impossible d'ouvrir cet emplacement.");
}

void MainWindow::exportCsv() {
    if (!hasScan() || busy()) return;
    const auto path = QFileDialog::getSaveFileName(this, "Exporter l'analyse", "disklens.csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export impossible", file.errorString()); return;
    }
    QTextStream stream(&file); stream.setEncoding(QStringConverter::Utf8);
    stream << "path,bytes,extension\n";
    for (const auto& node : scan_->nodes) {
        if (!node.directory) {
            auto text = displayPath(node.path);
            // Keep spreadsheet applications from interpreting a path as a formula.
            if (!text.isEmpty() && QString("=+-@").contains(text.front())) text.prepend('\'');
            stream << csvCell(text) << ',' << node.bytes << ','
                   << csvCell(displayPath(node.path.extension())) << '\n';
        }
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !file.commit())
        QMessageBox::warning(this, "Export impossible", file.errorString());
    else status_->setText("Export terminé : tous les fichiers de l'analyse, sans filtre.");
}

void MainWindow::showWarnings() {
    if (!scan_) return;
    QString text = QString("%1 accès ignorés. %2 liens / points de réanalyse ignorés.\n\n")
                       .arg(scan_->errors).arg(scan_->skippedLinks);
    for (const auto& warning : scan_->warnings) text += QString::fromUtf8(warning.c_str()) + '\n';
    if (scan_->errors > scan_->warnings.size()) text += "\nListe limitée aux 100 premiers incidents.";
    QMessageBox box(QMessageBox::Information, "Détails de l'analyse", "Accès ignorés", QMessageBox::Ok, this);
    box.setDetailedText(text); box.exec();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    event->accept();
}
} // namespace disklens
