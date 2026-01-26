/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
