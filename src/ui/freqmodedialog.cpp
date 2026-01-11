/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "freqmodedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>

FreqModeDialog::FreqModeDialog(QWidget *parent)
    : QDialog(parent)
    , m_frequency(14250.0)
    , m_mode("USB")
{
    setWindowTitle("Set Frequency and Mode");
    setupUi();
}

void FreqModeDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout();
    
    // Frequency input
    m_freqEdit = new QLineEdit(this);
    m_freqEdit->setPlaceholderText("e.g., 14250.0 or 14250000");
    formLayout->addRow("Frequency (kHz):", m_freqEdit);
    
    // Mode selector
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItems({"LSB", "USB", "CW", "FM", "AM", "RTTY", "DIG"});
    formLayout->addRow("Mode:", m_modeCombo);
    
    mainLayout->addLayout(formLayout);
    
    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &FreqModeDialog::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &FreqModeDialog::onCancelClicked);
    
    mainLayout->addWidget(buttonBox);
    
    setMinimumWidth(300);
}

void FreqModeDialog::setFrequency(double freqKhz)
{
    m_frequency = freqKhz;
    m_freqEdit->setText(QString::number(freqKhz, 'f', 1));
}

void FreqModeDialog::setMode(const QString& mode)
{
    m_mode = mode;
    int index = m_modeCombo->findText(mode, Qt::MatchFixedString);
    if (index >= 0) {
        m_modeCombo->setCurrentIndex(index);
    }
}

double FreqModeDialog::frequency() const
{
    return m_frequency;
}

QString FreqModeDialog::mode() const
{
    return m_mode;
}

void FreqModeDialog::onOkClicked()
{
    // Parse frequency in kHz (as labeled on the dialog)
    QString freqText = m_freqEdit->text().trimmed();
    bool ok;
    double freq = freqText.toDouble(&ok);
    
    if (!ok || freq <= 0) {
        // Invalid input - keep original
        reject();
        return;
    }
    
    // User enters frequency in kHz - use as-is
    m_frequency = freq;
    m_mode = m_modeCombo->currentText();
    
    accept();
}

void FreqModeDialog::onCancelClicked()
{
    reject();
}
