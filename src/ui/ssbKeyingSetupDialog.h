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

#ifndef SSBKEYINGSETUPDIALOG_H
#define SSBKEYINGSETUPDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

class PiperManager;

class SsbKeyingSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SsbKeyingSetupDialog(QWidget *parent = nullptr);

private slots:
    void onInstallPiper();
    void onTestTts();
    void onDetectPlatform();
    void onOk();
    void onCancel();

private:
    void loadSettings();
    void saveSettings();
    QString detectTtsCommand();
    QString detectAudioCommand();

    QCheckBox *m_enabledCheckbox;
    QLineEdit *m_ttsCommandEdit;
    QLineEdit *m_ttsArgsEdit;
    QLineEdit *m_audioPlayCommandEdit;
    QLineEdit *m_audioPlayArgsEdit;
    QTextEdit *m_helpText;
    QPushButton *m_installButton;
    QPushButton *m_testButton;
    QPushButton *m_detectButton;
    PiperManager *m_piperManager;
};

#endif // SSBKEYINGSETUPDIALOG_H
