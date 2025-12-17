#include "qsoeditdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>

QsoEditDialog::QsoEditDialog(const QsoRecord& qso, QWidget* parent)
    : QDialog(parent), m_originalQso(qso), m_editedQso(qso)
{
    setWindowTitle("Edit QSO");
    setModal(true);
    setupUi();
    loadQsoData();
}

void QsoEditDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QFormLayout* formLayout = new QFormLayout();

    m_dateTimeEdit = new QDateTimeEdit();
    m_dateTimeEdit->setDateTime(m_originalQso.getDateTime());
    formLayout->addRow("Date/Time:", m_dateTimeEdit);

    m_callEdit = new QLineEdit();
    m_callEdit->setText(m_originalQso.getCall());
    formLayout->addRow("Call:", m_callEdit);

    m_freqEdit = new QDoubleSpinBox();
    m_freqEdit->setRange(0, 10000000);
    m_freqEdit->setDecimals(1);
    m_freqEdit->setValue(m_originalQso.getFrequency().toDouble());
    formLayout->addRow("Frequency (kHz):", m_freqEdit);

    m_modeEdit = new QComboBox();
    m_modeEdit->addItems({"LSB", "USB", "CW", "RTTY", "FT8", "PSK31", "FT4", "JS8", "DIGI"});
    m_modeEdit->setCurrentText(m_originalQso.getMode());
    formLayout->addRow("Mode:", m_modeEdit);

    m_rstSentEdit = new QLineEdit();
    m_rstSentEdit->setText(m_originalQso.getRstSent());
    formLayout->addRow("RST Sent:", m_rstSentEdit);

    m_rstRecvEdit = new QLineEdit();
    m_rstRecvEdit->setText(m_originalQso.getRstReceived());
    formLayout->addRow("RST Received:", m_rstRecvEdit);

    m_exchSentEdit = new QLineEdit();
    m_exchSentEdit->setText(m_originalQso.getExchangeSent());
    formLayout->addRow("Exchange Sent:", m_exchSentEdit);

    m_exchRecvEdit = new QLineEdit();
    m_exchRecvEdit->setText(m_originalQso.getExchangeReceived());
    formLayout->addRow("Exchange Received:", m_exchRecvEdit);

    m_commentEdit = new QLineEdit();
    m_commentEdit->setText(m_originalQso.getComment());
    formLayout->addRow("Comment:", m_commentEdit);

    mainLayout->addLayout(formLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("OK");
    QPushButton* cancelBtn = new QPushButton("Cancel");

    connect(okBtn, &QPushButton::clicked, this, &QsoEditDialog::onOkClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QsoEditDialog::onCancelClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
    resize(400, 300);
}

void QsoEditDialog::loadQsoData()
{
    // Data is already loaded in setupUi
}

void QsoEditDialog::onOkClicked()
{
    m_editedQso.setDateTime(m_dateTimeEdit->dateTime());
    m_editedQso.setCall(m_callEdit->text());
    m_editedQso.setFrequency(QString::number(m_freqEdit->value()));
    m_editedQso.setMode(m_modeEdit->currentText());
    m_editedQso.setRstSent(m_rstSentEdit->text());
    m_editedQso.setRstReceived(m_rstRecvEdit->text());
    m_editedQso.setExchangeSent(m_exchSentEdit->text());
    m_editedQso.setExchangeReceived(m_exchRecvEdit->text());
    m_editedQso.setComment(m_commentEdit->text());

    accept();
}

void QsoEditDialog::onCancelClicked()
{
    reject();
}

QsoRecord QsoEditDialog::getEditedQso() const
{
    return m_editedQso;
}
