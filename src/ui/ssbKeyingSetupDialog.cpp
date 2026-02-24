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

#include "ssbKeyingSetupDialog.h"
#include "settings.h"
#include "debugLogger.h"
#include "piperManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QCoreApplication>

SsbKeyingSetupDialog::SsbKeyingSetupDialog(QWidget *parent)
    : QDialog(parent)
    , m_piperManager(new PiperManager(this))
{
    setWindowTitle("SSB Voice Keying Setup");
    setModal(true);
    resize(700, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Enable checkbox
    m_enabledCheckbox = new QCheckBox("Enable SSB Voice Keying", this);
    mainLayout->addWidget(m_enabledCheckbox);

    // TTS settings group
    QGroupBox *ttsGroup = new QGroupBox("Text-to-Speech Settings", this);
    QGridLayout *ttsLayout = new QGridLayout(ttsGroup);

    ttsLayout->addWidget(new QLabel("TTS Command:"), 0, 0);
    m_ttsCommandEdit = new QLineEdit(this);
    ttsLayout->addWidget(m_ttsCommandEdit, 0, 1);

    ttsLayout->addWidget(new QLabel("TTS Arguments:"), 1, 0);
    m_ttsArgsEdit = new QLineEdit(this);
    m_ttsArgsEdit->setPlaceholderText("Use {OUTPUT} for output file path");
    ttsLayout->addWidget(m_ttsArgsEdit, 1, 1);

    mainLayout->addWidget(ttsGroup);

    // Audio playback settings group
    QGroupBox *audioGroup = new QGroupBox("Audio Playback Settings", this);
    QGridLayout *audioLayout = new QGridLayout(audioGroup);

    audioLayout->addWidget(new QLabel("Audio Command:"), 0, 0);
    m_audioPlayCommandEdit = new QLineEdit(this);
    audioLayout->addWidget(m_audioPlayCommandEdit, 0, 1);

    audioLayout->addWidget(new QLabel("Audio Arguments:"), 1, 0);
    m_audioPlayArgsEdit = new QLineEdit(this);
    m_audioPlayArgsEdit->setPlaceholderText("Use {FILE} for audio file path");
    audioLayout->addWidget(m_audioPlayArgsEdit, 1, 1);

    mainLayout->addWidget(audioGroup);

    // Help text
    m_helpText = new QTextEdit(this);
    m_helpText->setReadOnly(true);
    m_helpText->setMaximumHeight(150);
    m_helpText->setHtml(
        "<b>Platform-specific defaults:</b><br>"
        "<b>macOS:</b> TTS: 'say' (no args), Audio: 'afplay' (args: {FILE})<br>"
        "<b>Linux:</b> TTS: 'piper' (args: --model en_US-hfc_male-medium --output_file {OUTPUT}), "
        "Audio: 'paplay' (args: {FILE})<br>"
        "<b>Windows:</b> Not yet supported<br><br>"
        "<b>Template variables:</b><br>"
        "{OUTPUT} - Replaced with generated TTS output file path<br>"
        "{FILE} - Replaced with audio file path to play<br>"
        "{TEXT} - Replaced with the text to speak (for TTS commands that take text directly)"
    );
    mainLayout->addWidget(m_helpText);

    // Button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_installButton = new QPushButton("Install Piper", this);
    connect(m_installButton, &QPushButton::clicked, this, &SsbKeyingSetupDialog::onInstallPiper);
    buttonLayout->addWidget(m_installButton);

    m_detectButton = new QPushButton("Detect Platform Defaults", this);
    connect(m_detectButton, &QPushButton::clicked, this, &SsbKeyingSetupDialog::onDetectPlatform);
    buttonLayout->addWidget(m_detectButton);

    m_testButton = new QPushButton("Test TTS", this);
    connect(m_testButton, &QPushButton::clicked, this, &SsbKeyingSetupDialog::onTestTts);
    buttonLayout->addWidget(m_testButton);

    QPushButton *okButton = new QPushButton("OK", this);
    connect(okButton, &QPushButton::clicked, this, &SsbKeyingSetupDialog::onOk);
    buttonLayout->addWidget(okButton);

    QPushButton *cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &SsbKeyingSetupDialog::onCancel);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    // Load current settings
    loadSettings();
}

void SsbKeyingSetupDialog::loadSettings()
{
    Settings& settings = Settings::instance();

    m_enabledCheckbox->setChecked(settings.getSsbKeyingEnabled());
    m_ttsCommandEdit->setText(settings.getTtsCommand());
    m_ttsArgsEdit->setText(settings.getTtsArgs());
    m_audioPlayCommandEdit->setText(settings.getAudioPlayCommand());
    m_audioPlayArgsEdit->setText(settings.getAudioPlayArgs());

    DebugLogger::instance().log("SsbKeyingSetupDialog", "Settings loaded");
}

void SsbKeyingSetupDialog::saveSettings()
{
    Settings& settings = Settings::instance();

    settings.setSsbKeyingEnabled(m_enabledCheckbox->isChecked());
    settings.setTtsCommand(m_ttsCommandEdit->text().trimmed());
    settings.setTtsArgs(m_ttsArgsEdit->text().trimmed());
    settings.setAudioPlayCommand(m_audioPlayCommandEdit->text().trimmed());
    settings.setAudioPlayArgs(m_audioPlayArgsEdit->text().trimmed());
    settings.save();

    DebugLogger::instance().log("SsbKeyingSetupDialog",
        QString("Settings saved - Enabled: %1, TTS: %2, Audio: %3")
        .arg(m_enabledCheckbox->isChecked())
        .arg(m_ttsCommandEdit->text())
        .arg(m_audioPlayCommandEdit->text()));
}

QString SsbKeyingSetupDialog::detectTtsCommand()
{
#ifdef Q_OS_MACOS
    return "say";
#elif defined(Q_OS_LINUX)
    // Use ContestLogX's managed piper installation
    if (m_piperManager->isPiperInstalled()) {
        return m_piperManager->getPiperPath();
    }
    return "";
#else
    return "";
#endif
}

QString SsbKeyingSetupDialog::detectAudioCommand()
{
#ifdef Q_OS_MACOS
    return "afplay";
#elif defined(Q_OS_LINUX)
    // Check for paplay first (PulseAudio), then aplay (ALSA)
    QProcess which;
    which.start("which", QStringList() << "paplay");
    which.waitForFinished(1000);
    if (which.exitCode() == 0) {
        return "paplay";
    }

    which.start("which", QStringList() << "aplay");
    which.waitForFinished(1000);
    if (which.exitCode() == 0) {
        return "aplay";
    }
    return "";
#else
    return "";
#endif
}

void SsbKeyingSetupDialog::onDetectPlatform()
{
    DebugLogger::instance().log("SsbKeyingSetupDialog", "Detecting platform defaults");

    QString ttsCmd = detectTtsCommand();
    QString audioCmd = detectAudioCommand();

    if (!ttsCmd.isEmpty()) {
        m_ttsCommandEdit->setText(ttsCmd);
#ifdef Q_OS_MACOS
        m_ttsArgsEdit->setText("");
#else
        m_ttsArgsEdit->setText("--model en_US-hfc_male-medium --output_file {OUTPUT}");
#endif
    }

    if (!audioCmd.isEmpty()) {
        m_audioPlayCommandEdit->setText(audioCmd);
        m_audioPlayArgsEdit->setText("{FILE}");
    }

    QString message;
    if (!ttsCmd.isEmpty() && !audioCmd.isEmpty()) {
        message = QString("Detected TTS command: %1\nDetected audio command: %2")
            .arg(ttsCmd).arg(audioCmd);
    } else if (!ttsCmd.isEmpty()) {
        message = QString("Detected TTS command: %1\nNo audio command found").arg(ttsCmd);
    } else if (!audioCmd.isEmpty()) {
        message = QString("No TTS command found\nDetected audio command: %1").arg(audioCmd);
    } else {
        message = "No TTS or audio commands found on this system.\n\n"
                  "On Linux, click 'Install Piper' to install piper-tts in ContestLogX's\n"
                  "virtual environment. You'll also need paplay (or aplay) for audio.\n\n"
                  "  macOS: say and afplay (built-in)\n"
                  "  Windows: Not yet supported";
    }

    QMessageBox::information(this, "Platform Detection", message);
}

void SsbKeyingSetupDialog::onInstallPiper()
{
    DebugLogger::instance().log("SsbKeyingSetupDialog", "Installing piper");

    if (m_piperManager->isPiperInstalled()) {
        QMessageBox::information(this, "Piper Already Installed",
            QString("Piper is already installed at:\n%1")
            .arg(m_piperManager->getPiperPath()));
        return;
    }

    QProgressDialog progress("Installing piper-tts...", "Cancel", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    QString statusText = "Installing piper-tts...";

    bool success = m_piperManager->installPiper([&](const QString& message) {
        statusText = message;
        progress.setLabelText(message);
        QCoreApplication::processEvents();
    });

    if (success) {
        QMessageBox::information(this, "Installation Complete",
            QString("Piper installed successfully!\n\nPath: %1\n\n"
                    "Click 'Detect Platform Defaults' to configure.")
            .arg(m_piperManager->getPiperPath()));
    } else {
        QMessageBox::warning(this, "Installation Failed",
            "Failed to install piper-tts.\n\n"
            "Please ensure Python 3 is installed:\n"
            "  python3 --version\n\n"
            "Last status: " + statusText);
    }
}

void SsbKeyingSetupDialog::onTestTts()
{
    DebugLogger::instance().log("SsbKeyingSetupDialog", "Testing TTS configuration");

    QString ttsCmd = m_ttsCommandEdit->text().trimmed();
    QString ttsArgs = m_ttsArgsEdit->text().trimmed();
    QString audioCmd = m_audioPlayCommandEdit->text().trimmed();
    QString audioArgs = m_audioPlayArgsEdit->text().trimmed();

    if (ttsCmd.isEmpty()) {
        QMessageBox::warning(this, "Test Failed", "TTS command is empty");
        return;
    }

    // Generate test output file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString outputFile = tempDir + "/clx_test_tts.wav";

    QString testText = "Contest Log X test transmission";

    // Build TTS command
    QStringList ttsCmdArgs;

#ifdef Q_OS_MACOS
    // macOS 'say' command takes text directly
    if (ttsCmd == "say") {
        ttsCmdArgs << "-o" << outputFile << "--data-format=LEF32@22050" << testText;
    }
#else
    // Linux piper takes text on stdin and uses args - add --data-dir to find downloaded models
    if (ttsCmd.contains("piper")) {
        ttsCmdArgs << "--data-dir" << m_piperManager->getVoiceModelDir();
    }
    if (!ttsArgs.isEmpty()) {
        QString argsStr = ttsArgs;
        argsStr.replace("{OUTPUT}", outputFile);
        ttsCmdArgs.append(argsStr.split(' ', Qt::SkipEmptyParts));
    }
#endif

    // Run TTS command
    QProcess ttsProcess;

    DebugLogger::instance().log("SsbKeyingSetupDialog",
        QString("Running TTS: %1 %2").arg(ttsCmd).arg(ttsCmdArgs.join(" ")));

#ifdef Q_OS_MACOS
    ttsProcess.start(ttsCmd, ttsCmdArgs);
#else
    ttsProcess.start(ttsCmd, ttsCmdArgs);
    ttsProcess.write(testText.toUtf8());
    ttsProcess.closeWriteChannel();
#endif

    if (!ttsProcess.waitForFinished(10000)) {
        QMessageBox::warning(this, "Test Failed", "TTS command timed out");
        ttsProcess.kill();
        return;
    }

    if (ttsProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(ttsProcess.readAllStandardError());

        // Check if error is about missing voice model
        if (error.contains("Unable to find voice:") && ttsCmd.contains("piper")) {
            // Extract model name from error or arguments
            QString modelName;
            QRegularExpression modelRegex("Unable to find voice: ([\\w_-]+)");
            QRegularExpressionMatch match = modelRegex.match(error);
            if (match.hasMatch()) {
                modelName = match.captured(1);
            } else {
                // Try to extract from --model argument
                QRegularExpression argRegex("--model\\s+([\\w_-]+)");
                QRegularExpressionMatch argMatch = argRegex.match(ttsArgs);
                if (argMatch.hasMatch()) {
                    modelName = argMatch.captured(1);
                }
            }

            if (!modelName.isEmpty()) {
                DebugLogger::instance().log("SsbKeyingSetupDialog",
                    QString("Piper voice model not found: %1").arg(modelName));

                QMessageBox::StandardButton reply = QMessageBox::question(this,
                    "Voice Model Not Found",
                    QString("The piper voice model '%1' is not installed.\n\n"
                            "Would you like to download it now?\n\n"
                            "This may take a few minutes depending on your connection.")
                    .arg(modelName),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QProgressDialog progress("Downloading voice model...", "Cancel", 0, 0, this);
                    progress.setWindowModality(Qt::WindowModal);
                    progress.setMinimumDuration(0);
                    progress.setValue(0);

                    bool success = m_piperManager->downloadVoiceModel(modelName, [&](const QString& message) {
                        progress.setLabelText(message);
                        QCoreApplication::processEvents();
                    });

                    if (success) {
                        QMessageBox::information(this, "Download Complete",
                            "Voice model downloaded successfully.\nYou can now test TTS.");
                    } else {
                        QMessageBox::warning(this, "Download Failed",
                            "Failed to download voice model.\nCheck the debug log for details.");
                    }
                    // Don't continue with test after download, user can test again
                    return;
                }
            }
        }

        QMessageBox::warning(this, "Test Failed",
            QString("TTS command failed with exit code %1:\n%2")
            .arg(ttsProcess.exitCode()).arg(error));
        return;
    }

    // Check if output file was created
    if (!QFile::exists(outputFile)) {
        QMessageBox::warning(this, "Test Failed",
            QString("TTS output file not created: %1").arg(outputFile));
        return;
    }

    // Test audio playback
    if (audioCmd.isEmpty()) {
        QMessageBox::information(this, "Test Partial Success",
            QString("TTS succeeded, but no audio command configured.\n"
                    "Output file: %1").arg(outputFile));
        return;
    }

    QString audioCmdArgs = audioArgs;
    audioCmdArgs.replace("{FILE}", outputFile);
    QStringList audioCmdArgsList = audioCmdArgs.split(' ', Qt::SkipEmptyParts);

    DebugLogger::instance().log("SsbKeyingSetupDialog",
        QString("Running audio: %1 %2").arg(audioCmd).arg(audioCmdArgsList.join(" ")));

    QProcess audioProcess;
    audioProcess.start(audioCmd, audioCmdArgsList);

    if (!audioProcess.waitForFinished(10000)) {
        QMessageBox::warning(this, "Test Failed", "Audio command timed out");
        audioProcess.kill();
        return;
    }

    if (audioProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(audioProcess.readAllStandardError());
        QMessageBox::warning(this, "Test Failed",
            QString("Audio command failed with exit code %1:\n%2")
            .arg(audioProcess.exitCode()).arg(error));
        return;
    }

    // Clean up test file
    QFile::remove(outputFile);

    QMessageBox::information(this, "Test Successful",
        "TTS and audio playback test completed successfully!");
}

void SsbKeyingSetupDialog::onOk()
{
    saveSettings();
    accept();
}

void SsbKeyingSetupDialog::onCancel()
{
    reject();
}

