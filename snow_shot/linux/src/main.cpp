// SPDX-License-Identifier: GPL-3.0-or-later
#include "widgets.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QScreen>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QStyle>
#include <QTimer>
#include <QTranslator>
#include <QVBoxLayout>

using namespace snow_shot::linux_preview;

class Launcher final : public QWidget {
    Q_OBJECT
public:
    Launcher() {
        setWindowTitle(tr("Snow Shot Linux Preview"));
        setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.pulipulichen.SnowShotLinux"),
            QApplication::style()->standardIcon(QStyle::SP_DesktopIcon)));
        auto* layout = new QVBoxLayout(this);
        auto* description = new QLabel(tr("X11 preview: capture, annotate, copy, save and pin."), this);
        description->setWordWrap(true);
        layout->addWidget(description);
        layout->addWidget(new QLabel(tr("Capture delay (seconds)"), this));
        delay_ = new QSpinBox(this);
        delay_->setRange(0, 30);
        layout->addWidget(delay_);
        auto* captureButton = new QPushButton(tr("New screenshot"), this);
        layout->addWidget(captureButton);
        QObject::connect(captureButton, &QPushButton::clicked, this, &Launcher::capture);
        auto* info = new QLabel(tr("For a global shortcut, assign snow-shot-linux --capture in KDE System Settings."), this);
        info->setWordWrap(true);
        layout->addWidget(info);
        auto* quit = new QPushButton(tr("Quit"), this);
        QObject::connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
        layout->addWidget(quit);
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            tray_ = new QSystemTrayIcon(windowIcon(), this);
            auto* menu = new QMenu(this);
            QObject::connect(menu->addAction(tr("New screenshot")), &QAction::triggered, this, &Launcher::capture);
            QObject::connect(menu->addAction(tr("Show window")), &QAction::triggered, this, &Launcher::reveal);
            menu->addSeparator();
            QObject::connect(menu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);
            tray_->setContextMenu(menu);
            tray_->setToolTip(windowTitle());
            tray_->show();
            QObject::connect(tray_, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason == QSystemTrayIcon::Trigger) {
                        capture();
                    }
                });
        }
        resize(440, 250);
    }
    void reveal() {
        show();
        raise();
        activateWindow();
    }
    void capture() {
        if (capturing_) {
            return;
        }
        capturing_ = true;
        // Hide every window owned by this app before freezing the desktop.
        hidden_.clear();
        for (auto* window : QApplication::topLevelWidgets()) {
            if (window->isVisible() && window->windowType() != Qt::Popup) {
                hidden_.append(window);
                window->hide();
            }
        }
        QTimer::singleShot(250 + delay_->value() * 1000, this, [this] {
            QVector<ScreenImage> screens;
            for (auto* screen : QGuiApplication::screens()) {
                screens.append({screen->geometry(), screen->grabWindow(0).toImage()});
            }
            DesktopImage desktop = composeDesktop(screens);
            if (desktop.image.isNull()) {
                restore();
                QMessageBox::warning(this, tr("Capture failed"),
                    tr("Could not capture all displays. Use a Plasma X11 session and try again."));
                return;
            }
            auto* overlay = new SelectionOverlay(std::move(desktop));
            QObject::connect(overlay, &SelectionOverlay::canceled, this, &Launcher::restore);
            QObject::connect(overlay, &SelectionOverlay::selected, this, [this](QImage image, qreal scale) {
                restore();
                auto* editor = new Editor(std::move(image), scale);
                editor->show();
                editor->raise();
                editor->activateWindow();
            });
            overlay->show();
            overlay->raise();
            overlay->activateWindow();
            overlay->setFocus();
        });
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        if (tray_) {
            event->ignore();
            hide();
        } else {
            QWidget::closeEvent(event);
        }
    }
private:
    void restore() {
        for (const auto& window : hidden_) {
            if (window) {
                window->show();
            }
        }
        hidden_.clear();
        capturing_ = false;
        if (!tray_ && !isVisible()) {
            show();
        }
    }
    QSpinBox* delay_;
    QSystemTrayIcon* tray_ = nullptr;
    QVector<QPointer<QWidget>> hidden_;
    bool capturing_ = false;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SnowShotLinux"));
    QCoreApplication::setApplicationName(QStringLiteral("snow-shot-linux"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("snow_shot_linux"), QStringLiteral("_"),
            QStringLiteral(":/i18n"))) {
        app.installTranslator(&translator);
    }
    QCommandLineParser parser;
    parser.setApplicationDescription(Launcher::tr("Experimental Snow Shot Linux frontend for Plasma X11."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("capture"), Launcher::tr("Start a region capture.")});
    parser.addOption({QStringLiteral("tray"), Launcher::tr("Start in the system tray.")});
    parser.process(app);
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        QMessageBox::critical(nullptr, Launcher::tr("X11 session required"),
            Launcher::tr("This preview requires Plasma X11. Wayland capture is not implemented yet."));
        return 2;
    }

    // User runtime storage + UserAccessOption prevent cross-user IPC. Separate
    // display names allow independent instances in separate X11 sessions.
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString session = QString::fromLatin1(QCryptographicHash::hash(qgetenv("DISPLAY"),
        QCryptographicHash::Sha256).toHex().left(16));
    const QString socketPath = runtime + QStringLiteral("/snow-shot-linux-") + session;
    QLockFile lock(socketPath + QStringLiteral(".lock"));
    const bool capture = parser.isSet(QStringLiteral("capture"));
    if (!lock.tryLock(1000)) {
        QLocalSocket socket;
        socket.connectToServer(socketPath);
        if (!socket.waitForConnected(3000)) {
            QMessageBox::warning(nullptr, Launcher::tr("Already running"),
                Launcher::tr("Another instance is starting or unavailable. Try again in a moment."));
            return 1;
        }
        socket.write(capture ? "capture\n" : "show\n");
        return socket.waitForBytesWritten(3000) ? 0 : 1;
    }
    QLocalServer::removeServer(socketPath); // Only after acquiring the process lock.
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(socketPath)) {
        qWarning() << "Local command server failed:" << server.errorString();
        QMessageBox::critical(nullptr, Launcher::tr("Startup failed"), server.errorString());
        return 1;
    }
    Launcher launcher;
    QObject::connect(&server, &QLocalServer::newConnection, &launcher, [&] {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            auto handle = [socket, &launcher] {
                if (socket->bytesAvailable() > 64) {
                    socket->disconnectFromServer();
                    return;
                }
                if (!socket->canReadLine()) {
                    return;
                }
                const QByteArray command = socket->readLine(64).trimmed();
                if (command == "capture") {
                    launcher.capture();
                } else if (command == "show") {
                    launcher.reveal();
                }
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, handle);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(5000, socket, &QObject::deleteLater);
            handle();
        }
    });
    app.setQuitOnLastWindowClosed(!QSystemTrayIcon::isSystemTrayAvailable());
    if (capture) {
        QTimer::singleShot(0, &launcher, &Launcher::capture);
    } else if (!parser.isSet(QStringLiteral("tray")) || !QSystemTrayIcon::isSystemTrayAvailable()) {
        launcher.show();
    }
    return app.exec();
}

#include "main.moc"
