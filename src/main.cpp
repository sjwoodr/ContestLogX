/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mainWindow.h"
#include "debugLogger.h"
#include "theme.h"
#include "settings.h"
#include "termsDialog.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include <QStyleFactory>
#include <QPalette>
#include <QIcon>

// Application version
static const char* APP_VERSION = "0.6.13";

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
    if (theme == "light") {
        qApp->setPalette(QApplication::style()->standardPalette());
    } else {
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

    parser.process(app);

    // Check if --debug flag is set
    debugToStdout = parser.isSet(debugOption);

    // Initialize debug logger FIRST — it owns the debug log file for the lifetime of the process
    DebugLogger::instance().init(parser.value(debugLogOption));
    DebugLogger::instance().setStdoutEnabled(debugToStdout);
    DebugLogger::instance().loadSettings();
    if (parser.isSet(flushOption))
        DebugLogger::instance().setFlushEnabled(true);

    // Route Qt framework messages (qDebug/qWarning etc.) through the same handler.
    // They write to stdout when --debug is set; DebugLogger owns the log file.
    qInstallMessageHandler(debugMessageHandler);
    DebugLogger::instance().log("INFO", QString("ContestLogX v%1 started").arg(APP_VERSION));
    
    applyTheme();

    // Show terms of use on first run (or when settings are reset)
    if (!parser.isSet(testOnlyOption) && !Settings::instance().getTermsAccepted()) {
        DebugLogger::instance().log("INFO", "Terms of use not yet accepted — showing dialog");
        TermsDialog terms;
        if (terms.exec() != QDialog::Accepted) {
            DebugLogger::instance().log("INFO", "User declined terms of use — exiting");
            return 1;
        }
        DebugLogger::instance().log("INFO", "User accepted terms of use");
        Settings::instance().setTermsAccepted(true);
    } else if (!parser.isSet(testOnlyOption)) {
        DebugLogger::instance().log("INFO", "Terms of use previously accepted — skipping dialog");
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
