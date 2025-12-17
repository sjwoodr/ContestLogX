#ifndef QSOEDITDIALOG_H
#define QSOEDITDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateTimeEdit>
#include "qsorecord.h"

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
    QLineEdit* m_rstSentEdit;
    QLineEdit* m_rstRecvEdit;
    QLineEdit* m_exchSentEdit;
    QLineEdit* m_exchRecvEdit;
    QLineEdit* m_commentEdit;
};

#endif // QSOEDITDIALOG_H
