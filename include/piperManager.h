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

#ifndef PIPERMANAGER_H
#define PIPERMANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>

/**
 * @brief Manages piper installation in ContestLogX's own virtual environment
 *
 * Creates and manages a Python venv at ~/.config/ContestLogX/venv
 * Installs piper-tts and manages voice model downloads
 */
class PiperManager : public QObject
{
    Q_OBJECT

public:
    explicit PiperManager(QObject *parent = nullptr);

    /**
     * @brief Check if piper is installed in our venv
     */
    bool isPiperInstalled() const;

    /**
     * @brief Check if a specific voice model is installed
     */
    bool isVoiceModelInstalled(const QString& modelName) const;

    /**
     * @brief Get path to piper executable in our venv
     */
    QString getPiperPath() const;

    /**
     * @brief Get path to Python executable in our venv
     */
    QString getPythonPath() const;

    /**
     * @brief Get path to voice models directory
     */
    QString getVoiceModelDir() const;

    /**
     * @brief Install piper-tts in our venv (creates venv if needed)
     * @param progressCallback Called with progress messages during installation
     * @return true if successful
     */
    bool installPiper(std::function<void(const QString&)> progressCallback = nullptr);

    /**
     * @brief Download a voice model
     * @param modelName Name of the model (e.g. "en_US-hfc_male-medium")
     * @param progressCallback Called with progress messages during download
     * @return true if successful
     */
    bool downloadVoiceModel(const QString& modelName,
                           std::function<void(const QString&)> progressCallback = nullptr);

signals:
    void installationProgress(const QString& message);
    void installationComplete();
    void installationError(const QString& error);

private:
    QString getVenvPath() const;
    QString getConfigPath() const;
    bool createVenv();
    bool ensureVenvExists();

    QString m_venvPath;
};

#endif // PIPERMANAGER_H
