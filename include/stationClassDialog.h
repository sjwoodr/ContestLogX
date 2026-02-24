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
