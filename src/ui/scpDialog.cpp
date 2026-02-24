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

#include "scpDialog.h"
#include "scpWidget.h"
#include "superCheckPartial.h"
#include "settings.h"
#include "debugLogger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFile>
#include <QMessageBox>

ScpDialog::ScpDialog(ScpWidget *scpWidget, QWidget *parent)
    : QDialog(parent), m_scpWidget(scpWidget)
{
    setupUi();
    updateDatabaseInfo();
}

void ScpDialog::setupUi()
{
    setWindowTitle("Super Check Partial (SCP)");
    setMinimumWidth(500);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Information group
    QGroupBox *infoGroup = new QGroupBox("Database Information", this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    
    m_databaseInfoLabel = new QLabel(this);
    infoLayout->addWidget(m_databaseInfoLabel);
    mainLayout->addWidget(infoGroup);
    
    // Download group
    QGroupBox *downloadGroup = new QGroupBox("Download Latest Database", this);
    QVBoxLayout *downloadLayout = new QVBoxLayout(downloadGroup);
    
    m_downloadButton = new QPushButton("Download master.scp", this);
    connect(m_downloadButton, &QPushButton::clicked, this, &ScpDialog::onDownloadClicked);
    downloadLayout->addWidget(m_downloadButton);
    
    m_statusLabel = new QLabel("Ready", this);
    downloadLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(downloadGroup);
    
    // Settings group
    QGroupBox *settingsGroup = new QGroupBox("SCP Settings", this);
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsGroup);
    
    m_enableCheckBox = new QCheckBox("Enable SCP during contest logging", this);
    connect(m_enableCheckBox, &QCheckBox::toggled, this, &ScpDialog::onScpToggled);
    m_enableCheckBox->setChecked(Settings::instance().getScpEnabled());
    settingsLayout->addWidget(m_enableCheckBox);
    mainLayout->addWidget(settingsGroup);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_closeButton = new QPushButton("Close", this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
}

void ScpDialog::updateDatabaseInfo()
{
    QString dataPath = SuperCheckPartial::instance().getDataFilePath();
    QFile file(dataPath);
    
    if (file.exists()) {
        SuperCheckPartial::instance().loadDatabase(dataPath);
        int size = SuperCheckPartial::instance().getDatabaseSize();
        m_databaseInfoLabel->setText(
            QString("Database file: %1\n"
                   "Loaded: %2 callsigns")
            .arg(dataPath)
            .arg(size));
    } else {
        m_databaseInfoLabel->setText(
            QString("Database file: %1\n"
                   "Status: Not found - please download")
            .arg(dataPath));
    }
}

void ScpDialog::onDownloadClicked()
{
    m_downloadButton->setEnabled(false);
    m_statusLabel->setText("Downloading...");
    
    QString targetPath = SuperCheckPartial::instance().getDataFilePath();
    QString errorMessage;
    
    if (SuperCheckPartial::downloadLatestDatabase(targetPath, errorMessage)) {
        m_statusLabel->setText("Download successful!");
        DebugLogger::instance().log("ScpDialog", "SCP database downloaded successfully");
        
        // Reload the database
        updateDatabaseInfo();
        
        // Update the SCP widget title since the database is now available
        if (m_scpWidget) {
            m_scpWidget->updateTitle();
        }
        
        QMessageBox::information(this, "Success", 
            QString("SCP database downloaded successfully!\n%1 callsigns loaded")
            .arg(SuperCheckPartial::instance().getDatabaseSize()));
    } else {
        m_statusLabel->setText("Download failed!");
        DebugLogger::instance().log("ScpDialog", "SCP download failed: " + errorMessage);
        QMessageBox::warning(this, "Download Failed", errorMessage);
    }
    
    m_downloadButton->setEnabled(true);
}

void ScpDialog::onScpToggled(bool checked)
{
    Settings::instance().setScpEnabled(checked);
    Settings::instance().save();
    DebugLogger::instance().log("ScpDialog", 
        QString("SCP %1").arg(checked ? "enabled" : "disabled"));
    
    if (m_scpWidget) {
        m_scpWidget->updateTitle();
    }
}

bool ScpDialog::isScpEnabled() const
{
    return m_enableCheckBox->isChecked();
}

void ScpDialog::setScpEnabled(bool enabled)
{
    m_enableCheckBox->setChecked(enabled);
}
