/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef STATIONCLASSDIALOG_H
#define STATIONCLASSDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QStringList>

class StationClassDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StationClassDialog(const QString& prompt, const QStringList& options, 
                                QWidget *parent = nullptr, const QString& defaultClass = QString());
    QString getSelectedClass() const;

private:
    QButtonGroup *m_buttonGroup;
    QString m_selectedClass;
};

#endif // STATIONCLASSDIALOG_H
