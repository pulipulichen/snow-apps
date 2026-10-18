// SPDX-License-Identifier: GPL-3.0-or-later
#include "image.h"
#include <QApplication>
#include <iostream>
#include <stdexcept>

using namespace snow_shot::linux_preview;
static void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
static QImage solid(QSize size, QColor color, qreal dpr = 1.0) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    image.setDevicePixelRatio(dpr);
    return image;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        // A monitor left of the primary plus a 2x monitor above it. QScreen
        // supplies logical geometries but grabWindow returns physical pixels.
        const DesktopImage desktop = composeDesktop({
            {QRect(-100, 0, 100, 100), solid(QSize(100, 100), Qt::red)},
            {QRect(0, -50, 100, 100), solid(QSize(200, 200), Qt::blue, 2.0)}});
        require(desktop.geometry == QRect(-100, -50, 200, 150), "virtual desktop origin");
        require(desktop.scale == 2.0, "preserve highest monitor pixel density");
        require(desktop.image.size() == QSize(400, 300), "desktop physical dimensions");
        require(desktop.image.pixelColor(100, 200) == QColor(Qt::red), "left monitor placement");
        require(desktop.image.pixelColor(300, 50) == QColor(Qt::blue), "upper monitor placement");
        require(desktop.image.pixelColor(50, 50) == QColor(Qt::black), "monitor gap fill");
        const QImage crop = cropSelection(desktop, QRectF(50, 60, 100, 20));
        require(crop.size() == QSize(200, 40), "cross-monitor crop dimensions");
        require(crop.pixelColor(10, 10) == QColor(Qt::red), "cross-monitor left pixels");
        require(crop.pixelColor(190, 10) == QColor(Qt::blue), "cross-monitor right pixels");
        require(cropSelection(desktop, QRectF(150, 80, -100, -20)) == crop,
            "drag in reverse direction");
        require(cropSelection(desktop, QRectF(-20, -20, 30, 30)).size() == QSize(20, 20),
            "clip selection at desktop bounds");
        require(cropSelection(desktop, QRectF(1, 1, 0, 10)).isNull(), "empty selection");
        require(composeDesktop({}).image.isNull(), "no displays");
        require(composeDesktop({{QRect(0, 0, 10, 10), QImage()}}).image.isNull(),
            "fail instead of omitting an uncapturable display");
        require(composeDesktop({{QRect(0, 0, 40000, 40000), solid(QSize(1, 1), Qt::red)}})
                    .image.isNull(), "allocation limit");
        QImage original = solid(QSize(80, 80), Qt::white);
        for (const Tool tool : {Tool::Pen, Tool::Rectangle, Tool::Ellipse, Tool::Arrow}) {
            Stroke stroke;
            stroke.tool = tool;
            stroke.points = {QPointF(10, 10), QPointF(60, 60)};
            const QImage annotated = applyStroke(original, stroke);
            require(annotated != original, "each annotation tool paints");
            require(original.pixelColor(10, 10) == QColor(Qt::white), "undo source remains unchanged");
            require(annotated.pixelColor(75, 75) == QColor(Qt::white), "unrelated pixels remain unchanged");
        }
        require(applyStroke(original, Stroke()) == original, "empty stroke is harmless");
        std::cout << "Linux image tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
