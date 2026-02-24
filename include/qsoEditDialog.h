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

#ifndef QSOEDITDIALOG_H
#define QSOEDITDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QMap>
#include "qsoRecord.h"

class QsoEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QsoEditDialog(const QsoRecord& qso, QWidget* parent = nullptr);
    QsoRecord getEditedQso() const;

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUi();
    void loadQsoData();

    QsoRecord m_originalQso;
    QsoRecord m_editedQso;

    QDateTimeEdit* m_dateTimeEdit;
    QLineEdit* m_callEdit;
    QDoubleSpinBox* m_freqEdit;
    QComboBox* m_modeEdit;
    QMap<QString, QLineEdit*> m_exchangeFieldEdits;
    QLineEdit* m_commentEdit;

    static QString exchangeFieldLabel(const QString& key);
};

#endif // QSOEDITDIALOG_H
