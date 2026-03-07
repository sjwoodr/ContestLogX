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

#include "dxClusterPanel.h"
#include "debugLogger.h"
#include "settings.h"
#include "../utils/bandPlan.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QTime>
#include <QTimer>
#include <QRegularExpression>
#include <QInputDialog>
#include <QJsonObject>
#include <QFontMetrics>
#include <QCheckBox>

DxClusterPanel::DxClusterPanel(QWidget *parent)
    : QWidget(parent)
    , m_spotTable(nullptr)
    , m_clusterEdit(nullptr)
    , m_connectButton(nullptr)
    , m_socket(new QTcpSocket(this))
    , m_propagationTimer(new QTimer(this))
    , m_expirationTimer(new QTimer(this))
    , m_isConnected(false)
    , m_loginSent(false)
{
    setupUi();
    loadSettings();
    
    connect(m_socket, &QTcpSocket::connected, this, &DxClusterPanel::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DxClusterPanel::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &DxClusterPanel::onSocketReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &DxClusterPanel::onSocketError);
    
    // Setup propagation timer - check every 15 minutes
    m_propagationTimer->setInterval(15 * 60 * 1000); // 15 minutes in milliseconds
    connect(m_propagationTimer, &QTimer::timeout, this, &DxClusterPanel::onPropagationTimerTimeout);
    
    // Setup expiration timer - check every minute for expired spots
    m_expirationTimer->setInterval(120 * 1000); // 2 minutes in milliseconds
    connect(m_expirationTimer, &QTimer::timeout, this, &DxClusterPanel::onExpireSpots);
    m_expirationTimer->start();
}

DxClusterPanel::~DxClusterPanel()
{
    saveSettings();
    m_propagationTimer->stop();
    m_expirationTimer->stop();
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void DxClusterPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(5);
    
    // Header with controls
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *titleLabel = new QLabel("<b>DX Cluster</b>");
    headerLayout->addWidget(titleLabel);
    
    m_clusterEdit = new QComboBox(this);
    m_clusterEdit->setEditable(true);
    m_clusterEdit->setMinimumWidth(200);
    m_clusterEdit->setMaximumWidth(260);
    headerLayout->addWidget(m_clusterEdit);
    
    m_connectButton = new QPushButton("Connect", this);
    m_connectButton->setMaximumWidth(80);
    connect(m_connectButton, &QPushButton::clicked, this, &DxClusterPanel::onConnect);
    headerLayout->addWidget(m_connectButton);
    
    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem("Spots");
    m_viewCombo->addItem("Console");
    m_viewCombo->setMaximumWidth(100);
    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DxClusterPanel::onViewChanged);
    headerLayout->addWidget(m_viewCombo);
    
    m_autoScrollCheckBox = new QCheckBox("Auto-scroll", this);
    m_autoScrollCheckBox->setChecked(true);
    m_autoScrollCheckBox->setMaximumWidth(100);
    headerLayout->addWidget(m_autoScrollCheckBox);
    
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);
    
    // Spots table view
    m_spotTable = new QTableWidget(0, 6, this);
    m_spotTable->setHorizontalHeaderLabels({"Time", "Callsign", "Frequency", "Mode", "Spotter", "Comment"});
    m_spotTable->horizontalHeader()->setStretchLastSection(true);
    m_spotTable->setAlternatingRowColors(true);
    m_spotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_spotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_spotTable->verticalHeader()->setVisible(false);
    
    // Set default font and darker alternating row colors
    QFont tableFont;
    tableFont.setPointSize(11);
    m_spotTable->setFont(tableFont);

    m_spotTable->setStyleSheet(
        "QTableWidget { "
        "  gridline-color: #404040; "
        "} "
        "QTableWidget::item:alternate { "
        "  background-color: #1a1a1a; "
        "} "
        "QTableWidget::item:selected { "
        "  background-color: #0066cc; "
        "  color: white; "
        "}"
    );
    m_spotTable->verticalHeader()->setDefaultSectionSize(28);
    m_spotTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    connect(m_spotTable, &QTableWidget::cellClicked, this, &DxClusterPanel::onSpotClicked);
    mainLayout->addWidget(m_spotTable);
    
    // Console text view
    m_consoleText = new QTextEdit(this);
    m_consoleText->setReadOnly(true);
    m_consoleText->setStyleSheet("QTextEdit { font-family: monospace; font-size: 9pt; }");
    m_consoleText->setVisible(false);
    mainLayout->addWidget(m_consoleText);
    
    // Command input
    QHBoxLayout *cmdLayout = new QHBoxLayout();
    QLabel *cmdLabel = new QLabel("DX Cluster Command");
    cmdLayout->addWidget(cmdLabel);
    
    m_commandEdit = new QLineEdit(this);
    m_commandEdit->setPlaceholderText("Enter cluster command...");
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &DxClusterPanel::onSendCommand);
    cmdLayout->addWidget(m_commandEdit);
    
    QPushButton *spotBtn = new QPushButton("Spot Last QSO", this);
    spotBtn->setMaximumWidth(120);
    connect(spotBtn, &QPushButton::clicked, this, &DxClusterPanel::onSpotLastQso);
    cmdLayout->addWidget(spotBtn);

    mainLayout->addLayout(cmdLayout);
}

void DxClusterPanel::onConnect()
{
    if (m_isConnected) {
        onDisconnect();
        return;
    }
    
    QString cluster = m_clusterEdit->currentText().trimmed();
    if (cluster.isEmpty()) {
        QMessageBox::warning(this, "DX Cluster", "Please enter a cluster address (host:port)");
        return;
    }
    
    QStringList parts = cluster.split(":");
    if (parts.size() != 2) {
        QMessageBox::warning(this, "DX Cluster", "Invalid format. Use: host:port");
        return;
    }
    
    QString host = parts[0];
    int port = parts[1].toInt();
    
    DebugLogger::instance().log("DxCluster", QString("Connecting to DX Cluster: %1:%2").arg(host).arg(port));
    m_socket->connectToHost(host, port);
    m_connectButton->setEnabled(false);
    m_connectButton->setText("Connecting...");
}

void DxClusterPanel::onDisconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    m_isConnected = false;
    m_loginSent = false;
    m_propagationTimer->stop();
    m_connectButton->setText("Connect");
    m_connectButton->setEnabled(true);
}

void DxClusterPanel::onSocketConnected()
{
    DebugLogger::instance().log("DxCluster", "DX Cluster connected");
    m_isConnected = true;
    m_loginSent = false;
    m_loginBuffer.clear();
    m_connectButton->setText("Disconnect");
    m_connectButton->setEnabled(true);
    
    // Get callsign from settings
    Settings& settings = Settings::instance();
    QString callsign = settings.getCallsign();
    
    if (callsign.isEmpty()) {
        // Will show login dialog when server responds
    } else {
        // Auto-login with configured callsign after a short delay
        QTimer::singleShot(1000, this, [this, callsign]() {
            m_socket->write((callsign + "\r\n").toUtf8());
            m_socket->flush();
            m_loginSent = true;
            DebugLogger::instance().log("DxCluster", QString("Sent auto-login: %1").arg(callsign));
            
            // Request propagation data after login
            QTimer::singleShot(2000, this, [this]() {
                m_socket->write("sh/wm\r\n");
                m_socket->flush();
                // Also request spots
                m_socket->write("sh/dx\r\n");
                m_socket->flush();
            });
        });
    }
}

void DxClusterPanel::onSocketDisconnected()
{
    DebugLogger::instance().log("DxCluster", "DX Cluster disconnected");
    m_isConnected = false;
    m_loginSent = false;
    m_loginBuffer.clear();
    m_connectButton->setText("Connect");
    m_connectButton->setEnabled(true);
}

void DxClusterPanel::onSocketReadyRead()
{
    while (m_socket->canReadLine()) {
        QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        DebugLogger::instance().log("DxCluster", line);
        
        // Add to console view
        m_consoleText->append(line);
        
        // If not logged in yet, accumulate buffer and show login dialog
        if (!m_loginSent) {
            m_loginBuffer += line + "\n";
            
            // Check if server is asking for login (common patterns)
            if (line.contains("login:", Qt::CaseInsensitive) || 
                line.contains("Please enter your call", Qt::CaseInsensitive) ||
                line.contains("callsign", Qt::CaseInsensitive) ||
                m_loginBuffer.length() > 200) {  // Or after receiving some data
                showLoginDialog();
            }
            return;
        }
        
        // Parse propagation data line
        // Format: Date        Hour   SFI   A   K Forecast                              Logger
        //         14-Dec-2025   15   122  14   1 No Storms -> No Storms                <W0MU>
        QRegularExpression propRegex("\\d{2}-\\w{3}-\\d{4}\\s+\\d+\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s");
        QRegularExpressionMatch propMatch = propRegex.match(line);
        if (propMatch.hasMatch()) {
            int sfi = propMatch.captured(1).toInt();
            int aIndex = propMatch.captured(2).toInt();
            int kIndex = propMatch.captured(3).toInt();
            DebugLogger::instance().log("DxCluster", QString("Propagation data: SFI=%1 A=%2 K=%3").arg(sfi).arg(aIndex).arg(kIndex));
            emit propagationDataReceived(sfi, aIndex, kIndex);
        }
        
        // Simple DX spot parsing (format: DX de SPOTTER: FREQ CALL COMMENT TIMESTAMP)
        // Example: DX de N9OH:     14025.0  W1AW       CQ CQ CQ               1659Z
        if (line.startsWith("DX de ")) {
            QStringList parts = line.mid(6).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString spotter = parts[0].replace(":", "");
                double freq = parts[1].toDouble();
                QString call = parts[2];
                QString comment = parts.mid(3).join(" ");
                
                // Strip trailing timestamp (format: NNNNZ at the end)
                QRegularExpression timeRegex("\\s+\\d{4}Z\\s*$");
                comment = comment.replace(timeRegex, "").trimmed();
                
                addSpot(call, freq, spotter, comment);
            }
        }
    }
}

void DxClusterPanel::onSocketError()
{
    DebugLogger::instance().log("DxCluster", QString("DX Cluster error: %1").arg(m_socket->errorString()));
    QMessageBox::critical(this, "DX Cluster Error", m_socket->errorString());
    onDisconnect();
}

void DxClusterPanel::onViewChanged(int index)
{
    // Toggle between Spots (table) and Console (text) view
    m_spotTable->setVisible(index == 0);
    m_consoleText->setVisible(index == 1);
}

void DxClusterPanel::onSendCommand()
{
    QString command = m_commandEdit->text().trimmed();
    if (command.isEmpty() || !m_isConnected) {
        return;
    }
    
    DebugLogger::instance().log("DxCluster", QString("Sending command: %1").arg(command));
    m_socket->write((command + "\r\n").toUtf8());
    m_commandEdit->clear();
}

void DxClusterPanel::addSpot(const QString& callsign, double frequency, const QString& spotter, const QString& comment)
{
    // Save the currently selected row before adding a new spot
    int selectedRow = -1;
    QList<QTableWidgetSelectionRange> selections = m_spotTable->selectedRanges();
    if (!selections.isEmpty()) {
        selectedRow = selections.first().topRow();
    }
    
    int row = m_spotTable->rowCount();
    m_spotTable->insertRow(row);
    
    auto createItem = [](const QString& text) {
        QTableWidgetItem* item = new QTableWidgetItem(text.trimmed());
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return item;
    };
    
    m_spotTable->setItem(row, 0, createItem(QTime::currentTime().toString("HH:mm:ss")));
    m_spotTable->setItem(row, 1, createItem(callsign));
    m_spotTable->setItem(row, 2, createItem(QString::number(frequency, 'f', 1)));
    
    // Calculate mode from frequency using band plan
    QString mode = BandPlan::freq2Mode(frequency / 1000.0); // Convert kHz to MHz
    m_spotTable->setItem(row, 3, createItem(mode));
    
    m_spotTable->setItem(row, 4, createItem(spotter));
    m_spotTable->setItem(row, 5, createItem(comment));
    
    // Store frequency and mode in the row for click handling
    m_spotTable->item(row, 2)->setData(Qt::UserRole, frequency);
    m_spotTable->item(row, 3)->setData(Qt::UserRole, mode);
    
    // Keep only last 100 spots
    if (m_spotTable->rowCount() > 100) {
        m_spotTable->removeRow(0);
        // If we removed row 0 and had a selection, adjust the selected row index
        if (selectedRow > 0) {
            selectedRow--;
        }
    }
    
    // Restore the selection if there was one
    if (selectedRow >= 0 && selectedRow < m_spotTable->rowCount()) {
        m_spotTable->selectRow(selectedRow);
    }
    
    // Only scroll if auto-scroll is enabled
    if (m_autoScrollCheckBox && m_autoScrollCheckBox->isChecked()) {
        m_spotTable->scrollToBottom();
    }
}

void DxClusterPanel::showLoginDialog()
{
    // Create custom dialog to show console and allow login
    QDialog *loginDialog = new QDialog(this);
    loginDialog->setWindowTitle("DX Cluster Login");
    loginDialog->setModal(true);
    loginDialog->resize(500, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(loginDialog);
    
    QTextEdit *consoleDisplay = new QTextEdit(loginDialog);
    consoleDisplay->setReadOnly(true);
    consoleDisplay->setPlainText(m_loginBuffer);
    consoleDisplay->setStyleSheet("QTextEdit { font-family: monospace; font-size: 9pt; }");
    layout->addWidget(consoleDisplay);
    
    QHBoxLayout *inputLayout = new QHBoxLayout();
    QLabel *label = new QLabel("Callsign:");
    inputLayout->addWidget(label);
    
    QLineEdit *callInput = new QLineEdit(loginDialog);
    callInput->setText(m_callsign);  // Use saved callsign if available
    inputLayout->addWidget(callInput);
    
    QPushButton *loginBtn = new QPushButton("Login", loginDialog);
    loginBtn->setDefault(true);
    inputLayout->addWidget(loginBtn);
    
    layout->addLayout(inputLayout);
    
    connect(loginBtn, &QPushButton::clicked, loginDialog, &QDialog::accept);
    connect(callInput, &QLineEdit::returnPressed, loginDialog, &QDialog::accept);
    
    if (loginDialog->exec() == QDialog::Accepted) {
        m_callsign = callInput->text().trimmed().toUpper();
        saveSettings();  // Save callsign for next time
        sendLoginAndCommands();
    } else {
        // User cancelled - disconnect
        onDisconnect();
    }
    
    loginDialog->deleteLater();
}

void DxClusterPanel::sendLoginAndCommands()
{
    if (m_callsign.isEmpty() || !m_isConnected) {
        return;
    }
    
    DebugLogger::instance().log("DxCluster", QString("Sending login: %1").arg(m_callsign));
    
    // Send callsign for login
    m_socket->write((m_callsign + "\r\n").toUtf8());
    m_socket->flush();
    
    m_loginSent = true;
    
    // After a short delay, send sh/dx and sh/wwv commands to get spots and propagation
    QTimer::singleShot(2000, this, [this]() {
        if (m_isConnected) {
            DebugLogger::instance().log("DxCluster", "Sending sh/dx command");
            m_socket->write("sh/dx\r\n");
            m_socket->flush();
            
            // Also get initial propagation data
            QTimer::singleShot(1000, this, [this]() {
                if (m_isConnected) {
                    DebugLogger::instance().log("DxCluster", "Sending sh/wwv command");
                    m_socket->write("sh/wwv\r\n");
                    m_socket->flush();
                }
            });
            
            // Start the periodic propagation timer
            m_propagationTimer->start();
        }
    });
}

void DxClusterPanel::loadSettings()
{
    Settings& settings = Settings::instance();

    m_clusterEdit->clear();
    const QStringList servers = settings.getDxClusterServers();
    for (const QString& srv : servers)
        m_clusterEdit->addItem(srv);

    // Restore last-used server as current selection
    QString lastServer = settings.getDxClusterServer();
    if (!lastServer.isEmpty()) {
        int idx = m_clusterEdit->findText(lastServer);
        if (idx >= 0)
            m_clusterEdit->setCurrentIndex(idx);
        else
            m_clusterEdit->setCurrentText(lastServer);
    }

    m_callsign = settings.getDxClusterCallsign();
}

void DxClusterPanel::saveSettings()
{
    Settings& settings = Settings::instance();
    settings.setDxClusterServer(m_clusterEdit->currentText());
    settings.setDxClusterCallsign(m_callsign);
}

void DxClusterPanel::onPropagationTimerTimeout()
{
    // Every 15 minutes, request updated propagation data
    if (m_isConnected && m_loginSent) {
        DebugLogger::instance().log("DxCluster", "Periodic propagation update - sending sh/wwv command");
        m_socket->write("sh/wwv\r\n");
        m_socket->flush();
    }
}

void DxClusterPanel::onSpotClicked(int row, int column)
{
    Q_UNUSED(column);
    
    // Get callsign, frequency and mode from the clicked row
    QTableWidgetItem *callItem = m_spotTable->item(row, 1);
    QTableWidgetItem *freqItem = m_spotTable->item(row, 2);
    QTableWidgetItem *modeItem = m_spotTable->item(row, 3);
    
    if (callItem && freqItem && modeItem) {
        QString callsign = callItem->text();
        double frequency = freqItem->data(Qt::UserRole).toDouble();
        QString mode = modeItem->data(Qt::UserRole).toString();
        
        DebugLogger::instance().log("DxCluster", QString("Spot clicked: call=%1 freq=%2 mode=%3").arg(callsign).arg(frequency).arg(mode));
        emit spotClicked(callsign, frequency, mode);
    }
}

void DxClusterPanel::removeSpot(const QString& callsign)
{
    for (int row = 0; row < m_spotTable->rowCount(); ++row) {
        QTableWidgetItem *callItem = m_spotTable->item(row, 1);
        if (callItem && callItem->text().toUpper() == callsign.toUpper()) {
            m_spotTable->removeRow(row);
            DebugLogger::instance().log("DxCluster", QString("Removed spot for %1").arg(callsign));
            return;
        }
    }
}

void DxClusterPanel::onExpireSpots()
{
    // Check for spots older than 15 minutes and remove them
    QTime currentTime = QTime::currentTime();
    const int EXPIRATION_MINUTES = 15;

    int expiredCount = 0;

    // Iterate backwards to avoid index issues when removing rows
    for (int row = m_spotTable->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *timeItem = m_spotTable->item(row, 0);
        if (timeItem) {
            QString timeStr = timeItem->text();
            QTime spotTime = QTime::fromString(timeStr, "HH:mm:ss");

            if (spotTime.isValid()) {
                // Calculate minutes old, handling day wrap (if spot time is later than current time, it's from yesterday)
                int minutesOld = spotTime.msecsTo(currentTime) / 60000;
                if (minutesOld < 0) {
                    // Day wrap occurred - spot is from yesterday
                    minutesOld = (24 * 60 * 60 * 1000 + spotTime.msecsTo(currentTime)) / 60000;
                }

                if (minutesOld > EXPIRATION_MINUTES) {
                    QTableWidgetItem *callItem = m_spotTable->item(row, 1);
                    QString callsign = callItem ? callItem->text() : "unknown";
                    m_spotTable->removeRow(row);
                    expiredCount++;
                    DebugLogger::instance().log("DxCluster",
                        QString("Expired spot for %1 (%2 minutes old)").arg(callsign).arg(minutesOld));
                }
            }
        }
    }

    if (expiredCount > 0) {
        DebugLogger::instance().log("DxCluster",
            QString("DX Cluster spot expiration: removed %1 spots older than %2 minutes").arg(expiredCount).arg(EXPIRATION_MINUTES));
    }
}

void DxClusterPanel::onSpotLastQso()
{
    // Request last QSO info from MainWindow via signal
    emit spotLastQsoRequested();
}

void DxClusterPanel::setTableFont(const QFont& font)
{
    m_spotTable->setFont(font);
    // Adjust row height proportionally to the new font size
    QFontMetrics fm(font);
    m_spotTable->verticalHeader()->setDefaultSectionSize(fm.height() + 10);
}

void DxClusterPanel::setSpotCommand(const QString& callsign, double freqKhz)
{
    // Format: dx <freq_in_khz> <callsign>
    QString command = QString("dx %1 %2").arg(freqKhz, 0, 'f', 1).arg(callsign);
    m_commandEdit->setText(command);
    m_commandEdit->setFocus();

    DebugLogger::instance().log("DxCluster", QString("Spot command prepared: %1").arg(command));
}
