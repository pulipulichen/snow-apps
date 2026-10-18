// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "image.h"
#include <QMainWindow>
#include <QWidget>
#include <functional>

namespace snow_shot::linux_preview {
class SelectionOverlay final : public QWidget {
    Q_OBJECT
public:
    explicit SelectionOverlay(DesktopImage desktop);
signals:
    void selected(QImage image, qreal scale);
    void canceled();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    DesktopImage desktop_;
    QPointF start_;
    QPointF end_;
    bool dragging_ = false;
};

class Canvas final : public QWidget {
    Q_OBJECT
public:
    Canvas(QImage image, qreal scale);
    const QImage& image() const { return image_; }
    void setTool(Tool tool) { stroke_.tool = tool; }
    void setColor(QColor color) { stroke_.color = color; }
    void setWidth(qreal width) { stroke_.width = width * scale_; }
    void undo();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    QPointF imagePoint(const QPointF& point) const;
    QImage image_;
    qreal scale_;
    Stroke stroke_;
    QVector<QImage> undo_;
    bool drawing_ = false;
};

class Editor final : public QMainWindow {
    Q_OBJECT
public:
    Editor(QImage image, qreal scale);
private:
    void save();
    void pin();
    Canvas* canvas_;
    qreal scale_;
};
} // namespace snow_shot::linux_preview
