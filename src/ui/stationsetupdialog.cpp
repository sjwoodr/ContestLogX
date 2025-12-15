/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "stationsetupdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

StationSetupDialog::StationSetupDialog(const StationInfo& info, QWidget *parent)
    : QDialog(parent)
    , m_stationInfo(info)
{
    setupUi();
}

StationSetupDialog::~StationSetupDialog()
{
}

void StationSetupDialog::setupUi()
{
    setWindowTitle("Station Setup");
    setMinimumWidth(500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create form layout
    QFormLayout* formLayout = new QFormLayout();
    
    m_callsignEdit = new QLineEdit(m_stationInfo.callsign(), this);
    m_callsignEdit->setMaxLength(20);
    connect(m_callsignEdit, &QLineEdit::textChanged, this, &StationSetupDialog::onCallsignTextChanged);
    formLayout->addRow("Callsign:", m_callsignEdit);
    
    m_nameEdit = new QLineEdit(m_stationInfo.operatorName(), this);
    formLayout->addRow("Operator Name:", m_nameEdit);
    
    m_gridEdit = new QLineEdit(m_stationInfo.grid(), this);
    m_gridEdit->setMaxLength(10);
    connect(m_gridEdit, &QLineEdit::textChanged, this, &StationSetupDialog::onGridTextChanged);
    formLayout->addRow("Grid Square:", m_gridEdit);
    
    m_addressEdit = new QLineEdit(m_stationInfo.state(), this);
    connect(m_addressEdit, &QLineEdit::textChanged, this, &StationSetupDialog::onStateTextChanged);
    formLayout->addRow("State/Province:", m_addressEdit);
    
    mainLayout->addLayout(formLayout);
    
    // Add button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &StationSetupDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
}

void StationSetupDialog::onAccept()
{
    m_stationInfo.setCallsign(m_callsignEdit->text().trimmed().toUpper());
    m_stationInfo.setOperatorName(m_nameEdit->text().trimmed());
    m_stationInfo.setGrid(m_gridEdit->text().trimmed().toUpper());
    m_stationInfo.setState(m_addressEdit->text().trimmed().toUpper());
    
    accept();
}

StationInfo StationSetupDialog::stationInfo() const
{
    return m_stationInfo;
}

void StationSetupDialog::onCallsignTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int cursorPos = m_callsignEdit->cursorPosition();
        m_callsignEdit->setText(upper);
        m_callsignEdit->setCursorPosition(cursorPos);
    }
}

void StationSetupDialog::onGridTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int cursorPos = m_gridEdit->cursorPosition();
        m_gridEdit->setText(upper);
        m_gridEdit->setCursorPosition(cursorPos);
    }
}

void StationSetupDialog::onStateTextChanged(const QString& text)
{
    QString upper = text.toUpper();
    if (text != upper) {
        int cursorPos = m_addressEdit->cursorPosition();
        m_addressEdit->setText(upper);
        m_addressEdit->setCursorPosition(cursorPos);
    }
}
