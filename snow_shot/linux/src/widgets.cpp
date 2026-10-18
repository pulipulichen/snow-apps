// SPDX-License-Identifier: GPL-3.0-or-later
#include "widgets.h"
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <algorithm>
#include <cmath>

namespace snow_shot::linux_preview {
SelectionOverlay::SelectionOverlay(DesktopImage desktop) : desktop_(std::move(desktop)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setCursor(Qt::CrossCursor);
    setGeometry(desktop_.geometry);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}
void SelectionOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.drawImage(rect(), desktop_.image);
    painter.fillRect(rect(), QColor(0, 0, 0, 100));
    const QRectF selection = QRectF(start_, end_).normalized().intersected(rect());
    if (dragging_ && !selection.isEmpty()) {
        painter.save();
        painter.setClipRect(selection);
        painter.drawImage(rect(), desktop_.image);
        painter.restore();
        painter.setPen(QPen(QColor(70, 170, 255), 2));
        painter.drawRect(selection);
    }
    painter.setPen(Qt::white);
    painter.fillRect(QRect(12, 12, 500, 40), QColor(0, 0, 0, 190));
    painter.drawText(QRect(24, 12, 475, 40), Qt::AlignVCenter,
        tr("Drag to select. Esc / right click: cancel."));
}
void SelectionOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        emit canceled();
        close();
    } else if (event->button() == Qt::LeftButton) {
        start_ = end_ = event->position();
        dragging_ = true;
        update();
    }
}
void SelectionOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        end_ = event->position();
        update();
    }
}
void SelectionOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !dragging_) {
        return;
    }
    end_ = event->position();
    QImage image = cropSelection(desktop_, QRectF(start_, end_));
    if (image.isNull()) {
        dragging_ = false;
        update();
        return;
    }
    emit selected(image, desktop_.scale);
    close();
}
void SelectionOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit canceled();
        close();
    }
}

Canvas::Canvas(QImage image, qreal scale) : image_(std::move(image)), scale_(scale) {
    image_.setDevicePixelRatio(1.0);
    setFixedSize(QSize(static_cast<int>(std::ceil(image_.width() / scale_)),
        static_cast<int>(std::ceil(image_.height() / scale_))));
    setCursor(Qt::CrossCursor);
    stroke_.width = 3 * scale_;
}
QPointF Canvas::imagePoint(const QPointF& point) const {
    return QPointF(std::clamp(point.x() * scale_, 0.0, image_.width() - 1.0),
        std::clamp(point.y() * scale_, 0.0, image_.height() - 1.0));
}
void Canvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.drawImage(rect(), drawing_ ? applyStroke(image_, stroke_) : image_);
}
void Canvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        stroke_.points = {imagePoint(event->position())};
        drawing_ = true;
        update();
    }
}
void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (!drawing_) {
        return;
    }
    if (stroke_.tool != Tool::Pen && stroke_.points.size() > 1) {
        stroke_.points.last() = imagePoint(event->position());
    } else {
        stroke_.points.append(imagePoint(event->position()));
    }
    update();
}
void Canvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !drawing_) {
        return;
    }
    stroke_.points.append(imagePoint(event->position()));
    // Keep image history within a predictable memory budget (implicit sharing
    // detaches only when painting the next committed image).
    while (!undo_.isEmpty() && (undo_.size() >= 16 ||
            image_.sizeInBytes() * (undo_.size() + 1) > 256 * 1024 * 1024)) {
        undo_.removeFirst();
    }
    if (image_.sizeInBytes() <= 256 * 1024 * 1024) {
        undo_.append(image_);
    }
    image_ = applyStroke(image_, stroke_);
    drawing_ = false;
    update();
}
void Canvas::undo() {
    if (!undo_.isEmpty() && !drawing_) {
        image_ = undo_.takeLast();
        update();
    }
}

// A pin is an independent image snapshot, so closing an editor cannot invalidate it.
class PinWindow final : public QWidget {
public:
    PinWindow(QImage image, qreal scale) : image_(std::move(image)) {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(Editor::tr("Pinned image"));
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        resize(QSize(std::max(1, static_cast<int>(image_.width() / scale)),
            std::max(1, static_cast<int>(image_.height() / scale))));
        setToolTip(Editor::tr("Drag to move. Esc / right click: close."));
        auto* shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        QObject::connect(shortcut, &QShortcut::activated, this, &QWidget::close);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawImage(rect(), image_);
        painter.setPen(QColor(70, 170, 255));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::RightButton) {
            close();
        } else if (event->button() == Qt::LeftButton) {
            offset_ = event->globalPosition().toPoint() - pos();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - offset_);
        }
    }
private:
    QImage image_;
    QPoint offset_;
};

Editor::Editor(QImage image, qreal scale) : canvas_(new Canvas(std::move(image), scale)), scale_(scale) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Snow Shot Linux Preview — Editor"));
    auto* scroll = new QScrollArea(this);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setWidget(canvas_);
    setCentralWidget(scroll);
    auto* toolbar = addToolBar(tr("Annotation"));
    toolbar->setMovable(false);
    const auto addTool = [this, toolbar](const QString& label, Tool tool) {
        QObject::connect(toolbar->addAction(label), &QAction::triggered, this,
            [this, tool] { canvas_->setTool(tool); });
    };
    addTool(tr("Pen"), Tool::Pen);
    addTool(tr("Rectangle"), Tool::Rectangle);
    addTool(tr("Ellipse"), Tool::Ellipse);
    addTool(tr("Arrow"), Tool::Arrow);
    QObject::connect(toolbar->addAction(tr("Color")), &QAction::triggered, this, [this] {
        QColor color = QColorDialog::getColor(Qt::red, this);
        if (color.isValid()) {
            canvas_->setColor(color);
        }
    });
    auto* width = new QSpinBox(toolbar);
    width->setRange(1, 30);
    width->setValue(3);
    width->setAccessibleName(tr("Line width"));
    toolbar->addWidget(width);
    QObject::connect(width, &QSpinBox::valueChanged, this,
        [this](int value) { canvas_->setWidth(value); });
    toolbar->addSeparator();
    auto* undo = toolbar->addAction(tr("Undo"));
    undo->setShortcut(QKeySequence::Undo);
    QObject::connect(undo, &QAction::triggered, canvas_, &Canvas::undo);
    auto* copy = toolbar->addAction(tr("Copy"));
    copy->setShortcut(QKeySequence::Copy);
    QObject::connect(copy, &QAction::triggered, this, [this] {
        QApplication::clipboard()->setImage(canvas_->image());
        statusBar()->showMessage(tr("Copied to clipboard."), 3000);
    });
    auto* saveAction = toolbar->addAction(tr("Save PNG"));
    saveAction->setShortcut(QKeySequence::Save);
    QObject::connect(saveAction, &QAction::triggered, this, &Editor::save);
    QObject::connect(toolbar->addAction(tr("Pin")), &QAction::triggered, this, &Editor::pin);
    resize(std::clamp(canvas_->width() + 40, 800, 1400),
        std::clamp(canvas_->height() + 100, 400, 900));
}
void Editor::save() {
    QSettings settings;
    const QString directory = settings.value(QStringLiteral("saveDirectory"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"),
        directory + QStringLiteral("/SnowShot-") +
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".png"),
        tr("PNG image (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".png");
    }
    QImageWriter writer(path, "png");
    if (!writer.write(canvas_->image())) {
        QMessageBox::warning(this, tr("Save failed"), writer.errorString());
    } else {
        settings.setValue(QStringLiteral("saveDirectory"), QFileInfo(path).absolutePath());
        statusBar()->showMessage(tr("Screenshot saved."), 3000);
    }
}
void Editor::pin() {
    auto* window = new PinWindow(canvas_->image(), scale_);
    window->move(pos() + QPoint(40, 80));
    window->show();
}
} // namespace snow_shot::linux_preview
