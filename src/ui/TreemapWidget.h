#pragma once

#include "core/TreemapLayout.h"
#include <QColor>
#include <QWidget>
#include <functional>
#include <memory>

namespace disklens {
QColor nodeColor(const Node& node);

class TreemapWidget : public QWidget {
public:
    explicit TreemapWidget(QWidget* parent = nullptr);
    void setDirectory(std::shared_ptr<const ScanResult> scan, NodeId directory);
    void setSelected(NodeId id);
    void setFilter(const QString& query);
    std::function<void(NodeId)> onSelected;
    std::function<void(NodeId)> onActivated;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void rebuild();
    NodeId hit(const QPointF& point) const;
    std::shared_ptr<const ScanResult> scan_;
    NodeId directory_ = NoNode;
    NodeId selected_ = NoNode;
    NodeId hovered_ = NoNode;
    QString query_;
    std::vector<MapRect> rectangles_;
};
} // namespace disklens
