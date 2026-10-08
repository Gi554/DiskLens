#include "ui/TreemapWidget.h"
#include "core/Path.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QTextOption>
#include <algorithm>

namespace disklens {
QColor nodeColor(const Node& node) {
    if (node.directory) {
        const auto variation = qHash(displayPath(node.path.filename())) % 28;
        return QColor::fromHsv(218 + static_cast<int>(variation), 155, 211);
    }
    const auto ext = displayPath(node.path.extension()).toLower();
    if (QStringList{".png", ".jpg", ".jpeg", ".webp", ".svg", ".gif", ".heic"}.contains(ext))
        return QColor("#16a98d");
    if (QStringList{".mp4", ".mkv", ".mov", ".mp3", ".wav", ".flac"}.contains(ext))
        return QColor("#aa64d6");
    if (QStringList{".zip", ".7z", ".rar", ".tar", ".gz"}.contains(ext))
        return QColor("#d89838");
    if (QStringList{".cpp", ".h", ".hpp", ".c", ".rs", ".js", ".ts", ".py"}.contains(ext))
        return QColor("#3a98c2");
    return QColor("#75849c");
}

TreemapWidget::TreemapWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(260, 120);
    setAccessibleName("Carte de l'espace utilisé");
}

void TreemapWidget::setDirectory(std::shared_ptr<const ScanResult> scan, NodeId directory) {
    scan_ = std::move(scan);
    directory_ = directory;
    selected_ = hovered_ = NoNode;
    rebuild();
}

void TreemapWidget::setSelected(NodeId id) { selected_ = id; update(); }
void TreemapWidget::setFilter(const QString& query) { query_ = query; update(); }

void TreemapWidget::rebuild() {
    rectangles_.clear();
    if (scan_ && directory_ < scan_->nodes.size()) {
        std::vector<MapItem> items;
        for (const auto id : scan_->nodes[directory_].children)
            items.push_back({id, scan_->nodes[id].bytes});
        rectangles_ = layoutTreemap(std::move(items), width() - 12, height() - 12);
    }
    update();
}

void TreemapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#eef1f7"));
    if (rectangles_.empty()) {
        painter.setPen(QColor("#657289"));
        QFont font = painter.font(); font.setPointSize(12); painter.setFont(font);
        const auto text = scan_ ? "Ce dossier ne contient aucun fichier non vide."
                                : "Choisissez un dossier pour visualiser son contenu.";
        painter.drawText(rect().adjusted(30, 30, -30, -30), Qt::AlignCenter | Qt::TextWordWrap,
                         QString::fromUtf8(text));
        return;
    }
    const auto& root = scan_->nodes[directory_];
    for (const auto& box : rectangles_) {
        const auto& node = scan_->nodes[box.id];
        const QRectF area(box.x + 7, box.y + 7, std::max(0.0, box.width - 2),
                          std::max(0.0, box.height - 2));
        if (area.width() < 0.5 || area.height() < 0.5) continue;
        QColor color = nodeColor(node);
        if (!query_.isEmpty() && !displayPath(node.path).contains(query_, Qt::CaseInsensitive))
            color.setAlpha(70);
        if (box.id == hovered_) color = color.lighter(112);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(area, std::min(6.0, area.width() / 4), std::min(6.0, area.height() / 4));
        if (box.id == selected_) {
            painter.setPen(QPen(QColor("#172445"), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(area.adjusted(2, 2, -2, -2), 4, 4);
        }
        if (area.width() > 65 && area.height() > 35) {
            painter.setPen(Qt::white);
            QFont font = painter.font(); font.setPointSize(10); font.setBold(true); painter.setFont(font);
            const auto name = displayPath(node.path.filename());
            const auto label = painter.fontMetrics().elidedText(name, Qt::ElideMiddle,
                                                                 static_cast<int>(area.width() - 20));
            painter.drawText(area.adjusted(10, 8, -10, -8), Qt::AlignTop | Qt::AlignLeft, label);
            if (area.height() > 66) {
                font.setBold(false); font.setPointSize(9); painter.setFont(font);
                const double share = root.bytes ? 100.0 * static_cast<double>(node.bytes) /
                                                    static_cast<double>(root.bytes) : 0;
                painter.drawText(area.adjusted(10, 30, -10, -8), Qt::AlignTop | Qt::AlignLeft,
                                  formatBytes(node.bytes) + " · " + QString::number(share, 'f', 1) + "%");
            }
        }
    }
}

void TreemapWidget::resizeEvent(QResizeEvent*) { rebuild(); }
NodeId TreemapWidget::hit(const QPointF& point) const {
    for (const auto& box : rectangles_) {
        if (QRectF(box.x + 6, box.y + 6, box.width, box.height).contains(point)) return box.id;
    }
    return NoNode;
}
void TreemapWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto id = hit(event->position());
    if (id != hovered_) { hovered_ = id; update(); }
    if (id != NoNode && scan_) {
        const auto& node = scan_->nodes[id];
        QToolTip::showText(event->globalPosition().toPoint(),
                          displayPath(node.path) + "\n" + formatBytes(node.bytes) +
                              (node.directory ? "\nDouble-clic pour explorer" : ""), this);
        setCursor(node.directory ? Qt::PointingHandCursor : Qt::ArrowCursor);
    } else { QToolTip::hideText(); unsetCursor(); }
}
void TreemapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        selected_ = hit(event->position());
        if (onSelected && selected_ != NoNode) onSelected(selected_);
        update();
    }
}
void TreemapWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const auto id = hit(event->position());
    if (event->button() == Qt::LeftButton && id != NoNode && onActivated) onActivated(id);
}
void TreemapWidget::leaveEvent(QEvent*) { hovered_ = NoNode; QToolTip::hideText(); update(); }
} // namespace disklens
