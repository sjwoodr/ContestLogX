/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "mainwindow.h"
#include "debuglogger.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QTimer>

// Application version
static const char* APP_VERSION = "0.0.7";

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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ContestLogX");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("ContestLogX");
    app.setOrganizationDomain("contestlogx.com");
    
    // Parse command-line arguments FIRST to check for --debug flag
    QCommandLineParser parser;
    parser.setApplicationDescription("Cross-platform amateur radio contest logging");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption debugOption("debug", "Write debug logs to stdout in addition to log file");
    parser.addOption(debugOption);
    
    QCommandLineOption logOption("log", "Load log file on startup", "filename");
    parser.addOption(logOption);
    
    parser.process(app);
    
    // Check if --debug flag is set
    debugToStdout = parser.isSet(debugOption);
    
    // Initialize debug logger FIRST
    DebugLogger::instance().init();
    
    // Enable stdout output if --debug flag is set
    DebugLogger::instance().setStdoutEnabled(debugToStdout);
    
    // Load debug settings BEFORE opening log file so filters are applied
    DebugLogger::instance().loadSettings();
    
    // Setup debug log file (truncate on each start)
    logFile = new QFile("clx_debug.log");
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        logStream = new QTextStream(logFile);
        qInstallMessageHandler(debugMessageHandler);
        DebugLogger::instance().log("INFO", QString("ContestLogX v%1 started - debug logging enabled").arg(APP_VERSION));
    } else {
        DebugLogger::instance().log("ERROR", "Could not open clx_debug.log for writing");
    }
    
    MainWindow window;
    window.show();
    
    // Load log file if specified via --log option
    if (parser.isSet(logOption)) {
        QString logFilename = parser.value(logOption);
        DebugLogger::instance().log("INFO", QString("Loading log file from command line: %1").arg(logFilename));
        window.loadLogFile(logFilename);
    }
    
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
