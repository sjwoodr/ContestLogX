/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SCPDIALOG_H
#define SCPDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

class ScpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScpDialog(QWidget *parent = nullptr);
    
    bool isScpEnabled() const;
    void setScpEnabled(bool enabled);

private slots:
    void onDownloadClicked();
    void onScpToggled(bool checked);

private:
    void setupUi();
    void updateDatabaseInfo();
    
    QPushButton *m_downloadButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QLabel *m_databaseInfoLabel;
    QCheckBox *m_enableCheckBox;
};

#endif // SCPDIALOG_H
