// SPDX-License-Identifier: GPL-3.0-or-later
#include "widgets.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QTest>

using namespace snow_shot::linux_preview;
class WidgetTests final : public QObject {
    Q_OBJECT
private slots:
    void annotateAndUndo() {
        QImage original(200, 200, QImage::Format_ARGB32_Premultiplied);
        original.fill(Qt::white);
        Canvas canvas(original, 2.0);
        canvas.show();
        QCOMPARE(canvas.size(), QSize(100, 100));
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QTest::mouseMove(&canvas, QPoint(60, 60));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 70));
        QVERIFY(canvas.image() != original);
        QCOMPARE(original.pixelColor(20, 20), QColor(Qt::white));
        canvas.undo();
        QCOMPARE(canvas.image(), original);
        canvas.undo(); // Empty undo is safe.
        QCOMPARE(canvas.image(), original);
    }
    void selectionAndCancel() {
        DesktopImage desktop;
        desktop.geometry = QRect(-50, 0, 100, 100);
        desktop.scale = 2;
        desktop.image = QImage(200, 200, QImage::Format_ARGB32_Premultiplied);
        desktop.image.fill(Qt::green);
        auto* overlay = new SelectionOverlay(desktop);
        QSignalSpy selected(overlay, &SelectionOverlay::selected);
        overlay->show();
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(80, 70));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
        QCOMPARE(selected.count(), 1);
        QCOMPARE(qvariant_cast<QImage>(selected.first().first()).size(), QSize(120, 120));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        overlay = new SelectionOverlay(desktop);
        QSignalSpy canceled(overlay, &SelectionOverlay::canceled);
        overlay->show();
        QTest::keyClick(overlay, Qt::Key_Escape);
        QCOMPARE(canceled.count(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void copyAndPin() {
        QImage image(120, 80, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        auto* editor = new Editor(image, 1.0);
        editor->show();
        bool copied = false;
        bool pinned = false;
        for (auto* action : editor->findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("Copy")) {
                action->trigger();
                copied = true;
            } else if (action->text() == QStringLiteral("Pin")) {
                action->trigger();
                pinned = true;
            }
        }
        QVERIFY(copied && pinned);
        QCOMPARE(QApplication::clipboard()->image(), image);
        QWidget* pin = nullptr;
        for (auto* window : QApplication::topLevelWidgets()) {
            if (window->windowTitle() == QStringLiteral("Pinned image")) {
                pin = window;
            }
        }
        QVERIFY(pin);
        QVERIFY(pin->windowFlags() & Qt::WindowStaysOnTopHint);
        editor->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(pin->isVisible()); // Pin owns its image independently.
        QTest::keyClick(pin, Qt::Key_Escape);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};
QTEST_MAIN(WidgetTests)
#include "widget_tests.moc"
