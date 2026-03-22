/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
