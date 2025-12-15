/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#ifndef STATIONSETUPDIALOG_H
#define STATIONSETUPDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include "stationinfo.h"

class StationSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StationSetupDialog(const StationInfo& info, QWidget *parent = nullptr);
    ~StationSetupDialog();
    
    StationInfo stationInfo() const;

private slots:
    void onAccept();
    void onCallsignTextChanged(const QString& text);
    void onGridTextChanged(const QString& text);
    void onStateTextChanged(const QString& text);

private:
    void setupUi();
    
    StationInfo m_stationInfo;
    QLineEdit* m_callsignEdit;
    QLineEdit* m_nameEdit;
    QLineEdit* m_gridEdit;
    QLineEdit* m_addressEdit;
};

#endif // STATIONSETUPDIALOG_H
