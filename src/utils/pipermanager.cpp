/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "pipermanager.h"
#include "debuglogger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>

PiperManager::PiperManager(QObject *parent)
    : QObject(parent)
{
    m_venvPath = getVenvPath();
}

QString PiperManager::getConfigPath() const
{
    // Use XDG_CONFIG_HOME or ~/.config/ContestLogX
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configPath + "/ContestLogX";
}

QString PiperManager::getVenvPath() const
{
    return getConfigPath() + "/venv";
}

QString PiperManager::getPythonPath() const
{
    return m_venvPath + "/bin/python";
}

QString PiperManager::getPiperPath() const
{
    return m_venvPath + "/bin/piper";
}

QString PiperManager::getVoiceModelDir() const
{
    return getConfigPath() + "/piper_models";
}

bool PiperManager::isPiperInstalled() const
{
    return QFile::exists(getPiperPath());
}

bool PiperManager::isVoiceModelInstalled(const QString& modelName) const
{
    // Piper downloads files as <modelName>.onnx directly in the model directory
    // Not in a subdirectory
    QString onnxFile = getVoiceModelDir() + "/" + modelName + ".onnx";
    QString jsonFile = getVoiceModelDir() + "/" + modelName + ".onnx.json";

    return QFile::exists(onnxFile) && QFile::exists(jsonFile);
}

bool PiperManager::ensureVenvExists()
{
    if (QFile::exists(getPythonPath())) {
        return true;
    }
    return createVenv();
}

bool PiperManager::createVenv()
{
    DebugLogger::instance().log("PiperManager", "Creating virtual environment");

    // Create config directory
    QDir().mkpath(getConfigPath());

    // Create venv using python3 -m venv
    QProcess venvProcess;
    venvProcess.start("python3", QStringList() << "-m" << "venv" << m_venvPath);

    if (!venvProcess.waitForStarted(5000)) {
        DebugLogger::instance().log("PiperManager", "Failed to start venv creation");
        return false;
    }

    if (!venvProcess.waitForFinished(30000)) {
        DebugLogger::instance().log("PiperManager", "Venv creation timed out");
        venvProcess.kill();
        return false;
    }

    if (venvProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(venvProcess.readAllStandardError());
        DebugLogger::instance().log("PiperManager",
            QString("Venv creation failed: %1").arg(error));
        return false;
    }

    DebugLogger::instance().log("PiperManager", "Virtual environment created successfully");
    return true;
}

bool PiperManager::installPiper(std::function<void(const QString&)> progressCallback)
{
    DebugLogger::instance().log("PiperManager", "Installing piper-tts");

    if (progressCallback) {
        progressCallback("Creating virtual environment...");
    }

    if (!ensureVenvExists()) {
        if (progressCallback) {
            progressCallback("ERROR: Failed to create virtual environment");
        }
        return false;
    }

    if (progressCallback) {
        progressCallback("Upgrading pip...");
    }

    // First upgrade pip in the venv
    QProcess pipUpgrade;
    pipUpgrade.start(getPythonPath(), QStringList() << "-m" << "pip" << "install" << "--upgrade" << "pip");

    if (pipUpgrade.waitForStarted(5000)) {
        pipUpgrade.waitForFinished(60000);
    }

    if (progressCallback) {
        progressCallback("Installing piper-tts (this may take a few minutes)...");
    }

    // Install piper-tts
    QProcess pipInstall;
    pipInstall.start(getPythonPath(),
                     QStringList() << "-m" << "pip" << "install" << "piper-tts");

    if (!pipInstall.waitForStarted(5000)) {
        DebugLogger::instance().log("PiperManager", "Failed to start pip install");
        if (progressCallback) {
            progressCallback("ERROR: Failed to start pip install");
        }
        return false;
    }

    // Monitor output
    while (pipInstall.state() == QProcess::Running) {
        pipInstall.waitForReadyRead(1000);
        QString output = QString::fromUtf8(pipInstall.readAllStandardOutput());
        QString errors = QString::fromUtf8(pipInstall.readAllStandardError());

        if (!output.isEmpty() && progressCallback) {
            // Show last line of output
            QStringList lines = output.trimmed().split('\n');
            if (!lines.isEmpty()) {
                progressCallback("Installing: " + lines.last());
            }
        }

        QCoreApplication::processEvents();

        if (!pipInstall.waitForFinished(1000)) {
            // Check if still running or timed out
            if (pipInstall.state() != QProcess::Running) {
                break;
            }
        } else {
            break;
        }
    }

    if (pipInstall.exitCode() != 0) {
        QString error = QString::fromUtf8(pipInstall.readAllStandardError());
        DebugLogger::instance().log("PiperManager",
            QString("Pip install failed: %1").arg(error));
        if (progressCallback) {
            progressCallback("ERROR: " + error);
        }
        return false;
    }

    DebugLogger::instance().log("PiperManager", "Piper installed successfully");
    if (progressCallback) {
        progressCallback("Piper installed successfully!");
    }

    return true;
}

bool PiperManager::downloadVoiceModel(const QString& modelName,
                                     std::function<void(const QString&)> progressCallback)
{
    DebugLogger::instance().log("PiperManager",
        QString("Downloading voice model: %1").arg(modelName));

    if (!isPiperInstalled()) {
        if (progressCallback) {
            progressCallback("ERROR: Piper is not installed");
        }
        return false;
    }

    // Create models directory
    QDir().mkpath(getVoiceModelDir());

    if (progressCallback) {
        progressCallback("Downloading voice model...");
    }

    // Download using: python -m piper.download_voices <model-name> --download-dir <data-dir>
    QProcess downloadProcess;
    QStringList args;
    args << "-m" << "piper.download_voices";
    args << modelName;  // Model name as positional argument
    args << "--download-dir" << getVoiceModelDir();
    args << "--debug";  // Enable debug output for troubleshooting

    DebugLogger::instance().log("PiperManager",
        QString("Running: %1 %2").arg(getPythonPath()).arg(args.join(" ")));

    downloadProcess.start(getPythonPath(), args);

    if (!downloadProcess.waitForStarted(5000)) {
        DebugLogger::instance().log("PiperManager", "Failed to start download");
        if (progressCallback) {
            progressCallback("ERROR: Failed to start download");
        }
        return false;
    }

    // Monitor download progress
    while (downloadProcess.state() == QProcess::Running) {
        downloadProcess.waitForReadyRead(1000);
        QString output = QString::fromUtf8(downloadProcess.readAllStandardOutput());
        QString errors = QString::fromUtf8(downloadProcess.readAllStandardError());

        // Show progress from either stdout or stderr
        QString progress = output.isEmpty() ? errors : output;
        if (!progress.trimmed().isEmpty() && progressCallback) {
            QStringList lines = progress.trimmed().split('\n');
            if (!lines.isEmpty()) {
                QString lastLine = lines.last();
                // Show percentage if available
                if (lastLine.contains('%')) {
                    progressCallback(lastLine);
                } else if (!lastLine.startsWith("DEBUG:")) {
                    progressCallback(lastLine);
                }
            }
        }

        QCoreApplication::processEvents();

        if (!downloadProcess.waitForFinished(1000)) {
            if (downloadProcess.state() != QProcess::Running) {
                break;
            }
        } else {
            break;
        }
    }

    // Read any remaining output
    QString finalOutput = QString::fromUtf8(downloadProcess.readAllStandardOutput());
    QString finalErrors = QString::fromUtf8(downloadProcess.readAllStandardError());

    DebugLogger::instance().log("PiperManager",
        QString("Download exit code: %1").arg(downloadProcess.exitCode()));
    DebugLogger::instance().log("PiperManager",
        QString("Download stdout: %1").arg(finalOutput));
    DebugLogger::instance().log("PiperManager",
        QString("Download stderr: %1").arg(finalErrors));

    if (downloadProcess.exitCode() != 0) {
        QString error = finalErrors.isEmpty() ? finalOutput : finalErrors;
        if (error.isEmpty()) {
            error = "Unknown error (no output captured)";
        }
        DebugLogger::instance().log("PiperManager",
            QString("Download failed with exit code %1: %2")
            .arg(downloadProcess.exitCode()).arg(error));
        if (progressCallback) {
            progressCallback("ERROR: " + error);
        }
        return false;
    }

    // Verify the model was actually downloaded
    QString onnxFile = getVoiceModelDir() + "/" + modelName + ".onnx";
    QString jsonFile = getVoiceModelDir() + "/" + modelName + ".onnx.json";

    DebugLogger::instance().log("PiperManager",
        QString("Checking for model files: %1, %2").arg(onnxFile).arg(jsonFile));

    bool onnxExists = QFile::exists(onnxFile);
    bool jsonExists = QFile::exists(jsonFile);

    DebugLogger::instance().log("PiperManager",
        QString("ONNX exists: %1, JSON exists: %2").arg(onnxExists).arg(jsonExists));

    if (!isVoiceModelInstalled(modelName)) {
        QString error = QString("Download completed but model files not found.\n"
                               "Expected files:\n  %1\n  %2")
                        .arg(onnxFile).arg(jsonFile);
        DebugLogger::instance().log("PiperManager", error);
        if (progressCallback) {
            progressCallback("ERROR: " + error);
        }
        return false;
    }

    DebugLogger::instance().log("PiperManager",
        QString("Voice model %1 downloaded successfully").arg(modelName));
    if (progressCallback) {
        progressCallback("Download complete!");
    }

    return true;
}
