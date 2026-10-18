// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>

namespace snow_shot::linux_preview {
struct ScreenImage {
    QRect geometry;
    QImage image;
};
struct DesktopImage {
    QRect geometry;
    QImage image;
    qreal scale = 1.0;
};
DesktopImage composeDesktop(const QVector<ScreenImage>& screens);
QImage cropSelection(const DesktopImage& desktop, const QRectF& selection);
enum class Tool { Pen, Rectangle, Ellipse, Arrow };
struct Stroke {
    Tool tool = Tool::Pen;
    QVector<QPointF> points;
    QColor color = QColor(240, 65, 65);
    qreal width = 3.0;
};
QImage applyStroke(const QImage& image, const Stroke& stroke);
} // namespace snow_shot::linux_preview
