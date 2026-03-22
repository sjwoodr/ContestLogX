/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef SCPDIALOG_H
#define SCPDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

class ScpWidget;

class ScpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScpDialog(ScpWidget *scpWidget, QWidget *parent = nullptr);
    
    bool isScpEnabled() const;
    void setScpEnabled(bool enabled);

private slots:
    void onDownloadClicked();
    void onScpToggled(bool checked);

private:
    void setupUi();
    void updateDatabaseInfo();
    
    ScpWidget *m_scpWidget;
    QPushButton *m_downloadButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QLabel *m_databaseInfoLabel;
    QCheckBox *m_enableCheckBox;
};

#endif // SCPDIALOG_H
