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

#include "cabrilloDialog.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QJsonObject>
#include <QScrollArea>
#include <QCloseEvent>

CabrilloDialog::CabrilloDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Export Cabrillo Log");
    setMinimumWidth(400);
    setupUI();
    loadFromSettings();
}

void CabrilloDialog::setupUI()
{
    setMinimumWidth(500);
    setMinimumHeight(700);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create a scrollable area for the fields
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    QWidget* scrollWidget = new QWidget();
    QVBoxLayout* formLayout = new QVBoxLayout(scrollWidget);
    
    // Callsign (read-only)
    QHBoxLayout* callLayout = new QHBoxLayout();
    callLayout->addWidget(new QLabel("Callsign:"));
    m_callsignEdit = new QLineEdit();
    m_callsignEdit->setReadOnly(true);
    callLayout->addWidget(m_callsignEdit);
    formLayout->addLayout(callLayout);
    
    // Operator
    QHBoxLayout* opLayout = new QHBoxLayout();
    opLayout->addWidget(new QLabel("Operators:"));
    m_operatorEdit = new QLineEdit();
    connect(m_operatorEdit, &QLineEdit::textChanged, this, [this]() {
        m_operatorEdit->blockSignals(true);
        m_operatorEdit->setText(m_operatorEdit->text().toUpper());
        m_operatorEdit->blockSignals(false);
    });
    connect(m_callsignEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_operatorEdit->text().isEmpty()) {
            m_operatorEdit->setText(m_callsignEdit->text());
        }
    });
    opLayout->addWidget(m_operatorEdit);
    formLayout->addLayout(opLayout);
    
    // Category
    QHBoxLayout* catLayout = new QHBoxLayout();
    catLayout->addWidget(new QLabel("Category:"));
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItems({"SINGLE-OP", "MULTI-OP", "CHECKLOG"});
    catLayout->addWidget(m_categoryCombo);
    formLayout->addLayout(catLayout);
    
    // Category Power
    QHBoxLayout* powerLayout = new QHBoxLayout();
    powerLayout->addWidget(new QLabel("Power:"));
    m_categoryPowerCombo = new QComboBox();
    m_categoryPowerCombo->addItems({"HIGH", "LOW", "QRP"});
    powerLayout->addWidget(m_categoryPowerCombo);
    formLayout->addLayout(powerLayout);
    
    // Category Mode
    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("Mode:"));
    m_categoryModeCombo = new QComboBox();
    m_categoryModeCombo->addItems({"CW", "SSB", "MIXED", "DIGITAL", "RTTY", "FT8"});
    modeLayout->addWidget(m_categoryModeCombo);
    formLayout->addLayout(modeLayout);
    
    // Category Operator
    QHBoxLayout* catOpLayout = new QHBoxLayout();
    catOpLayout->addWidget(new QLabel("Operator Type:"));
    m_categoryOperatorCombo = new QComboBox();
    m_categoryOperatorCombo->addItems({"SINGLE", "MULTI", "CHECKLOG"});
    catOpLayout->addWidget(m_categoryOperatorCombo);
    formLayout->addLayout(catOpLayout);
    
    // Category Band
    QHBoxLayout* catBandLayout = new QHBoxLayout();
    catBandLayout->addWidget(new QLabel("Band:"));
    m_categoryBandCombo = new QComboBox();
    m_categoryBandCombo->addItems({"ALL-BAND", "160M", "80M", "40M", "20M", "15M", "10M", "6M", "2M", "70CM"});
    catBandLayout->addWidget(m_categoryBandCombo);
    formLayout->addLayout(catBandLayout);
    
    // Category Transmitter
    QHBoxLayout* txLayout = new QHBoxLayout();
    txLayout->addWidget(new QLabel("Transmitter:"));
    m_categoryTransmitterCombo = new QComboBox();
    m_categoryTransmitterCombo->addItems({"ONE", "TWO", "LIMITED", "UNLIMITED"});
    txLayout->addWidget(m_categoryTransmitterCombo);
    formLayout->addLayout(txLayout);
    
    // Category Assisted
    QHBoxLayout* assistLayout = new QHBoxLayout();
    assistLayout->addWidget(new QLabel("Assisted:"));
    m_categoryAssistedCombo = new QComboBox();
    m_categoryAssistedCombo->addItems({"YES", "NO"});
    assistLayout->addWidget(m_categoryAssistedCombo);
    formLayout->addLayout(assistLayout);
    
    // Category Overlay
    QHBoxLayout* overlayLayout = new QHBoxLayout();
    overlayLayout->addWidget(new QLabel("Overlay:"));
    m_categoryOverlayCombo = new QComboBox();
    m_categoryOverlayCombo->addItems({"", "CLASSIC", "ROOKIE", "TB-WIRES", "YOUTH", "NOVICE-TECH", "YL"});
    overlayLayout->addWidget(m_categoryOverlayCombo);
    formLayout->addLayout(overlayLayout);
    
    // Name
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Name:"));
    m_nameEdit = new QLineEdit();
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() {
        m_nameEdit->blockSignals(true);
        m_nameEdit->setText(m_nameEdit->text().toUpper());
        m_nameEdit->blockSignals(false);
    });
    nameLayout->addWidget(m_nameEdit);
    formLayout->addLayout(nameLayout);
    
    // Address
    QHBoxLayout* addrLayout = new QHBoxLayout();
    addrLayout->addWidget(new QLabel("Address:"));
    m_addressEdit = new QLineEdit();
    connect(m_addressEdit, &QLineEdit::textChanged, this, [this]() {
        m_addressEdit->blockSignals(true);
        m_addressEdit->setText(m_addressEdit->text().toUpper());
        m_addressEdit->blockSignals(false);
    });
    addrLayout->addWidget(m_addressEdit);
    formLayout->addLayout(addrLayout);
    
    // Address City
    QHBoxLayout* cityLayout = new QHBoxLayout();
    cityLayout->addWidget(new QLabel("City:"));
    m_addressCityEdit = new QLineEdit();
    connect(m_addressCityEdit, &QLineEdit::textChanged, this, [this]() {
        m_addressCityEdit->blockSignals(true);
        m_addressCityEdit->setText(m_addressCityEdit->text().toUpper());
        m_addressCityEdit->blockSignals(false);
    });
    cityLayout->addWidget(m_addressCityEdit);
    formLayout->addLayout(cityLayout);
    
    // State/Province
    QHBoxLayout* stateLayout = new QHBoxLayout();
    stateLayout->addWidget(new QLabel("State/Province:"));
    m_stateEdit = new QLineEdit();
    connect(m_stateEdit, &QLineEdit::textChanged, this, [this]() {
        m_stateEdit->blockSignals(true);
        m_stateEdit->setText(m_stateEdit->text().toUpper());
        m_stateEdit->blockSignals(false);
    });
    stateLayout->addWidget(m_stateEdit);
    formLayout->addLayout(stateLayout);
    
    // Postal Code
    QHBoxLayout* postalLayout = new QHBoxLayout();
    postalLayout->addWidget(new QLabel("Postal Code:"));
    m_postalEdit = new QLineEdit();
    connect(m_postalEdit, &QLineEdit::textChanged, this, [this]() {
        m_postalEdit->blockSignals(true);
        m_postalEdit->setText(m_postalEdit->text().toUpper());
        m_postalEdit->blockSignals(false);
    });
    postalLayout->addWidget(m_postalEdit);
    formLayout->addLayout(postalLayout);
    
    // Country
    QHBoxLayout* countryLayout = new QHBoxLayout();
    countryLayout->addWidget(new QLabel("Country:"));
    m_countryEdit = new QLineEdit();
    connect(m_countryEdit, &QLineEdit::textChanged, this, [this]() {
        m_countryEdit->blockSignals(true);
        m_countryEdit->setText(m_countryEdit->text().toUpper());
        m_countryEdit->blockSignals(false);
    });
    countryLayout->addWidget(m_countryEdit);
    formLayout->addLayout(countryLayout);
    
    // Location
    QHBoxLayout* locLayout = new QHBoxLayout();
    locLayout->addWidget(new QLabel("Location:"));
    m_locationEdit = new QLineEdit();
    connect(m_locationEdit, &QLineEdit::textChanged, this, [this]() {
        m_locationEdit->blockSignals(true);
        m_locationEdit->setText(m_locationEdit->text().toUpper());
        m_locationEdit->blockSignals(false);
    });
    locLayout->addWidget(m_locationEdit);
    formLayout->addLayout(locLayout);
    
    // Club
    QHBoxLayout* clubLayout = new QHBoxLayout();
    clubLayout->addWidget(new QLabel("Club:"));
    m_clubEdit = new QLineEdit();
    connect(m_clubEdit, &QLineEdit::textChanged, this, [this]() {
        m_clubEdit->blockSignals(true);
        m_clubEdit->setText(m_clubEdit->text().toUpper());
        m_clubEdit->blockSignals(false);
    });
    clubLayout->addWidget(m_clubEdit);
    formLayout->addLayout(clubLayout);
    
    // Email
    QHBoxLayout* emailLayout = new QHBoxLayout();
    emailLayout->addWidget(new QLabel("Email:"));
    m_emailEdit = new QLineEdit();
    emailLayout->addWidget(m_emailEdit);
    formLayout->addLayout(emailLayout);
    
    // Claimed Score
    QHBoxLayout* scoreLayout = new QHBoxLayout();
    scoreLayout->addWidget(new QLabel("Claimed Score:"));
    m_claimedScoreLabel = new QLabel("0");
    scoreLayout->addWidget(m_claimedScoreLabel);
    scoreLayout->addStretch();
    formLayout->addLayout(scoreLayout);
    
    // Soapbox
    formLayout->addWidget(new QLabel("Soapbox:"));
    m_commentsEdit = new QTextEdit();
    m_commentsEdit->setMaximumHeight(100);
    formLayout->addWidget(m_commentsEdit);
    
    formLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* okBtn = new QPushButton("OK");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
    
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        saveToSettings();
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        saveToSettings();
        reject();
    });
}

QJsonObject CabrilloDialog::getHeaderData() const
{
    QJsonObject data;
    data["callsign"] = m_callsignEdit->text();
    data["operatorName"] = m_operatorEdit->text();
    data["category"] = m_categoryCombo->currentText();
    data["categoryPower"] = m_categoryPowerCombo->currentText();
    data["categoryMode"] = m_categoryModeCombo->currentText();
    data["categoryOperator"] = m_categoryOperatorCombo->currentText();
    data["categoryBand"] = m_categoryBandCombo->currentText();
    data["categoryTransmitter"] = m_categoryTransmitterCombo->currentText();
    data["categoryAssisted"] = m_categoryAssistedCombo->currentText();
    data["categoryOverlay"] = m_categoryOverlayCombo->currentText();
    data["name"] = m_nameEdit->text();
    data["address"] = m_addressEdit->text();
    data["addressCity"] = m_addressCityEdit->text();
    data["addressStateProvince"] = m_stateEdit->text();
    data["addressPostalcode"] = m_postalEdit->text();
    data["addressCountry"] = m_countryEdit->text();
    data["location"] = m_locationEdit->text();
    data["club"] = m_clubEdit->text();
    data["email"] = m_emailEdit->text();
    data["claimedScore"] = m_claimedScoreLabel->text();
    data["soapbox"] = m_commentsEdit->toPlainText();
    return data;
}

void CabrilloDialog::setCallsign(const QString& call)
{
    m_callsignEdit->setText(call);
}

void CabrilloDialog::setClaimedScore(int score)
{
    m_claimedScoreLabel->setText(QString::number(score));
}

void CabrilloDialog::loadFromSettings()
{
    Settings& settings = Settings::instance();
    
    // Operator field should always be the callsign
    m_operatorEdit->setText(m_callsignEdit->text());
    m_emailEdit->setText(settings.getCabrilloEmail());
    
    // Load state
    m_stateEdit->setText(settings.getState());
    
    // Load name and location from station info
    m_nameEdit->setText(settings.getOperatorName());
    m_locationEdit->setText(settings.getState());
    
    // Load all other persistent values from settings JSON
    m_addressCityEdit->setText(settings.getCabrilloAddressCity());
    m_addressEdit->setText(settings.getCabrilloAddress());
    m_postalEdit->setText(settings.getCabrilloPostalCode());
    m_countryEdit->setText(settings.getCabrilloCountry());
    m_clubEdit->setText(settings.getCabrilloClub());
    m_commentsEdit->setPlainText(settings.getCabrillSoapbox());
    m_emailEdit->setText(settings.getCabrilloEmail());
    
    // Load dropdown values
    m_categoryCombo->setCurrentText(settings.getCabrilloCategory());
    m_categoryPowerCombo->setCurrentText(settings.getCabrilloPower());
    m_categoryModeCombo->setCurrentText(settings.getCabrilloMode());
    m_categoryOperatorCombo->setCurrentText(settings.getCabrilloOperatorType());
    m_categoryBandCombo->setCurrentText(settings.getCabrilloBand());
    m_categoryTransmitterCombo->setCurrentText(settings.getCabrilloTransmitter());
    m_categoryAssistedCombo->setCurrentText(settings.getCabrilloAssisted());
    m_categoryOverlayCombo->setCurrentText(settings.getCabrilloOverlay());
    
    // Restore dialog geometry
    QByteArray savedGeometry = settings.getCabrilloDialogGeometry();
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }
}

void CabrilloDialog::saveToSettings() const
{
    Settings& settings = Settings::instance();
    settings.setCabrilloEmail(m_emailEdit->text());
    settings.setCabrilloAddressCity(m_addressCityEdit->text());
    settings.setCabrilloAddress(m_addressEdit->text());
    settings.setCabrilloPostalCode(m_postalEdit->text());
    settings.setCabrilloCountry(m_countryEdit->text());
    settings.setCabrilloClub(m_clubEdit->text());
    settings.setCabrillSoapbox(m_commentsEdit->toPlainText());
    settings.setCabrilloCategory(m_categoryCombo->currentText());
    settings.setCabrilloPower(m_categoryPowerCombo->currentText());
    settings.setCabrilloMode(m_categoryModeCombo->currentText());
    settings.setCabrilloOperatorType(m_categoryOperatorCombo->currentText());
    settings.setCabrilloBand(m_categoryBandCombo->currentText());
    settings.setCabrilloTransmitter(m_categoryTransmitterCombo->currentText());
    settings.setCabrilloAssisted(m_categoryAssistedCombo->currentText());
    settings.setCabrilloOverlay(m_categoryOverlayCombo->currentText());

    // Save dialog geometry
    settings.setCabrilloDialogGeometry(saveGeometry());
    settings.save();
}

void CabrilloDialog::closeEvent(QCloseEvent *event)
{
    saveToSettings();
    QDialog::closeEvent(event);
}
