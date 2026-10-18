// SPDX-License-Identifier: GPL-3.0-or-later
#include "image.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace snow_shot::linux_preview {
DesktopImage composeDesktop(const QVector<ScreenImage>& screens) {
    DesktopImage result;
    for (const auto& screen : screens) {
        if (screen.image.isNull() || screen.geometry.isEmpty()) {
            return {}; // Never silently deliver a screenshot with a missing monitor.
        }
        result.geometry = result.geometry.united(screen.geometry);
        result.scale = std::max(result.scale,
            static_cast<qreal>(screen.image.width()) / screen.geometry.width());
    }
    if (result.geometry.isEmpty()) {
        return {};
    }
    const qreal width = std::ceil(result.geometry.width() * result.scale);
    const qreal height = std::ceil(result.geometry.height() * result.scale);
    // Bound allocations, including arrangements with large gaps between displays.
    if (width > 32768 || height > 32768 || width * height > 100000000) {
        return {};
    }
    result.image = QImage(QSize(static_cast<int>(width), static_cast<int>(height)),
        QImage::Format_ARGB32_Premultiplied);
    result.image.fill(Qt::black);
    QPainter painter(&result.image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const auto& screen : screens) {
        const QRect relative = screen.geometry.translated(-result.geometry.topLeft());
        QImage pixels = screen.image;
        pixels.setDevicePixelRatio(1.0);
        painter.drawImage(QRectF(relative.x() * result.scale, relative.y() * result.scale,
                              relative.width() * result.scale, relative.height() * result.scale),
            pixels);
    }
    return result;
}

QImage cropSelection(const DesktopImage& desktop, const QRectF& selection) {
    const QRectF bounded = selection.normalized().intersected(
        QRectF(QPointF(0, 0), desktop.geometry.size()));
    if (bounded.width() < 1 || bounded.height() < 1 || desktop.image.isNull()) {
        return {};
    }
    const QRect pixels = QRectF(bounded.x() * desktop.scale, bounded.y() * desktop.scale,
        bounded.width() * desktop.scale, bounded.height() * desktop.scale).toAlignedRect();
    return desktop.image.copy(pixels.intersected(desktop.image.rect()));
}

QImage applyStroke(const QImage& source, const Stroke& stroke) {
    if (source.isNull() || stroke.points.isEmpty()) {
        return source;
    }
    QImage image = source;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF start = stroke.points.first();
    const QPointF end = stroke.points.last();
    const QRectF bounds = QRectF(start, end).normalized();
    switch (stroke.tool) {
    case Tool::Pen: {
        QPainterPath path(start);
        for (qsizetype i = 1; i < stroke.points.size(); ++i) {
            path.lineTo(stroke.points[i]);
        }
        if (stroke.points.size() == 1) {
            painter.drawPoint(start);
        } else {
            painter.drawPath(path);
        }
        break;
    }
    case Tool::Rectangle: painter.drawRect(bounds); break;
    case Tool::Ellipse: painter.drawEllipse(bounds); break;
    case Tool::Arrow: {
        painter.drawLine(start, end);
        const qreal length = std::hypot(end.x() - start.x(), end.y() - start.y());
        if (length < 1) {
            break;
        }
        const QPointF direction = (end - start) / length;
        const QPointF normal(-direction.y(), direction.x());
        const qreal head = std::min(length, std::max(12.0, stroke.width * 4));
        painter.drawLine(end, end - direction * head + normal * (head / 2));
        painter.drawLine(end, end - direction * head - normal * (head / 2));
        break;
    }
    }
    return image;
}
} // namespace snow_shot::linux_preview
