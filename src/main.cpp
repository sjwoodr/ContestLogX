/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "mainWindow.h"
#include "debugLogger.h"
#include "theme.h"
#include "settings.h"
#include "termsDialog.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include <QStyleFactory>
#include <QPalette>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

// Application version
static const char* APP_VERSION = "0.9.7";

// Bump this to force the terms dialog to re-appear for all users.
// History: 1 = original terms, 2 = MIT license change
static const int TERMS_VERSION = 2;

// Global log file
static QFile *logFile = nullptr;
static QTextStream *logStream = nullptr;
static bool debugToStdout = false;

// Custom message handler for general debugging
void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out flrig-related debug messages if flrig debug is disabled
    if (!DebugLogger::instance().isFlrigDebugEnabled() && type == QtDebugMsg) {
        // Check if this is a flrig-related message
        QString lowerMsg = msg.toLower();
        if (lowerMsg.contains("rig") || 
            lowerMsg.contains("frequency") ||
            lowerMsg.contains("mode") ||
            lowerMsg.contains("cwio") ||
            lowerMsg.contains("xml") ||
            lowerMsg.contains("flrig") ||
            lowerMsg.contains("sending") ||
            lowerMsg.contains("received") ||
            lowerMsg.contains("parsed") ||
            lowerMsg.contains("response buffer")) {
            return; // Skip this message
        }
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString typeStr;
    
    switch (type) {
        case QtDebugMsg:    typeStr = "DEBUG"; break;
        case QtInfoMsg:     typeStr = "INFO "; break;
        case QtWarningMsg:  typeStr = "WARN "; break;
        case QtCriticalMsg:  typeStr = "CRIT "; break;
        case QtFatalMsg:    typeStr = "FATAL"; break;
    }
    
    QString logMsg = QString("[%1] %2: %3\n").arg(timestamp, typeStr, msg);
    
    // Write to log file
    if (logStream) {
        *logStream << logMsg;
        logStream->flush();
    }
    
    // Output to stdout if --debug flag is set
    if (debugToStdout) {
        fprintf(stdout, "%s", logMsg.toLocal8Bit().constData());
        fflush(stdout);
    }
    
    // Also output to stderr for critical/fatal
    if (type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s", logMsg.toLocal8Bit().constData());
    }
}

void applyTheme()
{
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QString theme = Settings::instance().getTheme();
    // Use a stylesheet to force colors - this overrides the GNOME/GTK platform
    // theme which would otherwise ignore QPalette changes on dark desktops.
    if (theme == "light") {
        qApp->setStyleSheet(
            "QWidget { background-color: #f0f0f0; color: #000000; }"
            "QMainWindow { background-color: #f0f0f0; }"
            "QMenuBar { background-color: #f0f0f0; color: #000000; }"
            "QMenuBar::item:selected { background-color: #2a82da; color: #ffffff; }"
            "QMenu { background-color: #ffffff; color: #000000; }"
            "QMenu::item:selected { background-color: #2a82da; color: #ffffff; }"
            "QTableView, QTreeView, QListView, QTextEdit, QPlainTextEdit, QLineEdit {"
            "  background-color: #ffffff; color: #000000;"
            "  alternate-background-color: #f5f5f5; }"
            "QHeaderView::section { background-color: #e0e0e0; color: #000000; }"
            "QPushButton { background-color: #e0e0e0; color: #000000; }"
            "QComboBox { background-color: #ffffff; color: #000000; }"
            "QTabWidget::pane { background-color: #f0f0f0; }"
            "QTabBar::tab { background-color: #e0e0e0; color: #000000; }"
            "QTabBar::tab:selected { background-color: #f0f0f0; }"
            "QDockWidget { color: #000000; }"
            "QDockWidget::title { background-color: #e0e0e0; }"
            "QStatusBar { background-color: #f0f0f0; color: #000000; }"
            "QToolTip { background-color: #ffffdc; color: #000000; border: 1px solid #767676; }"
            "QSpinBox, QDoubleSpinBox { background-color: #ffffff; color: #000000; }"
            "QCheckBox, QRadioButton { color: #000000; }"
            "QGroupBox { color: #000000; }"
            "QLabel { color: #000000; }"
            "QScrollBar { background-color: #f0f0f0; }"
        );
    } else {
        qApp->setStyleSheet("");

        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        qApp->setPalette(darkPalette);
    }
}

int main(int argc, char *argv[])
{
    // Force X11/XCB backend if user enabled it in settings (Linux only).
    // Must be done before QApplication is constructed.
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        QString configDir = qgetenv("XDG_CONFIG_HOME");
        if (configDir.isEmpty())
            configDir = QDir::homePath() + "/.config";
        QFile settingsFile(configDir + "/ContestLogX/ContestLogX.json");
        if (settingsFile.open(QIODevice::ReadOnly)) {
            QJsonObject root = QJsonDocument::fromJson(settingsFile.readAll()).object();
            if (root["ui"].toObject()["forceX11"].toBool(true)) {
                qputenv("QT_QPA_PLATFORM", "xcb");
            }
            settingsFile.close();
        }
    }
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ContestLogX");
    app.setApplicationVersion(APP_VERSION);
    // Don't set organization name to avoid double "ContestLogX" in AppDataLocation path
    // app.setOrganizationName("ContestLogX");
    app.setOrganizationDomain("contestlogx.com");
    app.setDesktopFileName("ContestLogX.desktop");
    app.setWindowIcon(QIcon(":/contestlogx.png"));

    // Parse command-line arguments FIRST to check for --debug flag
    QCommandLineParser parser;
    parser.setApplicationDescription("Cross-platform amateur radio contest logging");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption debugOption("debug", "Write debug logs to stdout in addition to log file");
    parser.addOption(debugOption);
    
    QCommandLineOption logOption("log", "Load log file on startup", "filename");
    parser.addOption(logOption);
    
    QCommandLineOption testOnlyOption("test-only", "Test mode: load log and exit after calculating score");
    parser.addOption(testOnlyOption);

    QCommandLineOption flushOption("flush", "Flush debug log after every write (slower, for debugging hangs)");
    parser.addOption(flushOption);

    QCommandLineOption debugLogOption("debug-log", "Write debug log to this file instead of clx_debug.log", "path");
    parser.addOption(debugLogOption);

    QCommandLineOption configDirOption("config-dir",
        "Use the given directory for ContestLogX.json (and the QSettings INI "
        "used by Debug toggles) instead of the platform default. If the dir "
        "doesn't already contain ContestLogX.json, the real config is copied "
        "into it as a starting point so the sandbox session has full state. "
        "Use for local smoke testing without stomping your real config.",
        "path");
    parser.addOption(configDirOption);

    parser.process(app);

    // Check if --debug flag is set
    debugToStdout = parser.isSet(debugOption);

    // Apply --config-dir BEFORE the first Settings::instance() call. Both
    // Settings (JSON) and DebugLogger (QSettings INI) are redirected.
    // Pre-seed: if the sandbox dir doesn't already have a ContestLogX.json,
    // copy the user's real config into it so the sandbox starts with full
    // state (callsign, CW memories, station info, terms-accepted version,
    // saved layout, etc.) instead of looking like a first-run install.
    // The QSettings INI side gets the same dir via setPath(); we don't
    // copy the INI in (it's mostly Debug toggles, low-stakes if reset).
    if (parser.isSet(configDirOption)) {
        const QString sandboxDir = parser.value(configDirOption);
        QDir().mkpath(sandboxDir);
        const QString sandboxFile = QDir(sandboxDir).filePath("ContestLogX.json");
        if (!QFile::exists(sandboxFile)) {
            const QString realConfigRoot = QStandardPaths::writableLocation(
                QStandardPaths::ConfigLocation);
            const QString realFile = QDir(realConfigRoot)
                .filePath("ContestLogX/ContestLogX.json");
            if (QFile::exists(realFile)) {
                if (QFile::copy(realFile, sandboxFile)) {
                    fprintf(stderr,
                        "config-dir: seeded sandbox from %s -> %s\n",
                        realFile.toLocal8Bit().constData(),
                        sandboxFile.toLocal8Bit().constData());
                } else {
                    fprintf(stderr,
                        "config-dir: WARNING - failed to copy %s into sandbox; "
                        "CLX will start with default config\n",
                        realFile.toLocal8Bit().constData());
                }
            } else {
                fprintf(stderr,
                    "config-dir: no real config at %s; sandbox will start fresh\n",
                    realFile.toLocal8Bit().constData());
            }
        } else {
            fprintf(stderr, "config-dir: using existing sandbox config at %s\n",
                sandboxFile.toLocal8Bit().constData());
        }
        Settings::setOverrideConfigDir(sandboxDir);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, sandboxDir);
    }

    // Initialize debug logger FIRST - it owns the debug log file for the lifetime of the process
    DebugLogger::instance().init(parser.value(debugLogOption));
    DebugLogger::instance().setStdoutEnabled(debugToStdout);
    DebugLogger::instance().loadSettings();
    if (parser.isSet(flushOption))
        DebugLogger::instance().setFlushEnabled(true);

    // Route Qt framework messages (qDebug/qWarning etc.) through the same handler.
    // They write to stdout when --debug is set; DebugLogger owns the log file.
    qInstallMessageHandler(debugMessageHandler);
    DebugLogger::instance().log("INFO", QString("ContestLogX v%1 started").arg(APP_VERSION));
    if (qgetenv("QT_QPA_PLATFORM") == "xcb")
        DebugLogger::instance().log("INFO", "Force X11 backend enabled (QT_QPA_PLATFORM=xcb)");
    if (parser.isSet(configDirOption)) {
        DebugLogger::instance().log("INFO",
            QString("Using sandbox config dir: %1").arg(parser.value(configDirOption)));
    }

    applyTheme();

    // Show terms of use on first run or when TERMS_VERSION has been bumped
    int acceptedVersion = Settings::instance().getTermsAcceptedVersion();
    if (!parser.isSet(testOnlyOption) && acceptedVersion < TERMS_VERSION) {
        DebugLogger::instance().log("INFO",
            QString("Terms v%1 not yet accepted (have v%2) - showing dialog")
                .arg(TERMS_VERSION).arg(acceptedVersion));
        TermsDialog terms;
        if (terms.exec() != QDialog::Accepted) {
            DebugLogger::instance().log("INFO", "User declined terms of use - exiting");
            return 1;
        }
        DebugLogger::instance().log("INFO", QString("User accepted terms v%1").arg(TERMS_VERSION));
        Settings::instance().setTermsAcceptedVersion(TERMS_VERSION);
    } else if (!parser.isSet(testOnlyOption)) {
        DebugLogger::instance().log("INFO", "Terms of use previously accepted - skipping dialog");
    }

    MainWindow window;
    window.show();
    
    // Set test mode if --test-only flag is set
    if (parser.isSet(testOnlyOption)) {
        window.setTestMode(true);
    }
    
    // Log file loading is now handled in MainWindow constructor
    // (it was being called both from main.cpp and from the constructor)
    
    int result = app.exec();
    
    // Cleanup
    if (logStream) {
        delete logStream;
        logStream = nullptr;
    }
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}
