/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef QRZCQSETTINGSDIALOG_H
#define QRZCQSETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

class QrzcqApi;

class QrzcqSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QrzcqSettingsDialog(QWidget* parent = nullptr);
    ~QrzcqSettingsDialog();

private slots:
    void onTestConnection();
    void onSessionObtained(const QString& token);
    void onSessionError(const QString& error);
    void onAccepted();

private:
    void setupUI();
    void loadSettings();
    
    QCheckBox* m_autoLookupCheckbox;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_testButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    QrzcqApi* m_api;
};

#endif // QRZCQSETTINGSDIALOG_H
