/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
#include <QDateTime>
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
    , m_bandFilterCombo(nullptr)
    , m_modeFilterCombo(nullptr)
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
    
    m_clusterEdit = new QComboBox(this);
    m_clusterEdit->setEditable(true);
    m_clusterEdit->setMinimumWidth(200);
    m_clusterEdit->setMaximumWidth(260);
    connect(m_clusterEdit, &QComboBox::currentTextChanged,
            this, &DxClusterPanel::onClusterSelectionChanged);
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

    m_bandFilterCombo = new QComboBox(this);
    m_bandFilterCombo->addItem("ALL");
    m_bandFilterCombo->setMaximumWidth(80);
    connect(m_bandFilterCombo, &QComboBox::currentTextChanged,
            this, &DxClusterPanel::onBandFilterChanged);
    headerLayout->addWidget(m_bandFilterCombo);

    m_modeFilterCombo = new QComboBox(this);
    m_modeFilterCombo->addItems({"ALL", "CW", "SSB", "RTTY", "FTx"});
    m_modeFilterCombo->setMaximumWidth(70);
    connect(m_modeFilterCombo, &QComboBox::currentTextChanged,
            this, &DxClusterPanel::onModeFilterChanged);
    headerLayout->addWidget(m_modeFilterCombo);

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
    m_connectedServer = cluster;
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
    m_connectedServer.clear();
    m_propagationTimer->stop();
    m_connectButton->setText("Connect");
    m_connectButton->setEnabled(true);
}

void DxClusterPanel::onSocketConnected()
{
    DebugLogger::instance().log("DxCluster", "DX Cluster connected");
    m_isConnected = true;
    emit clusterConnectedChanged(true);
    m_loginSent = false;
    m_loginBuffer.clear();
    m_spotTable->setRowCount(0);
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
    emit clusterConnectedChanged(false);
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
        
        // Parse propagation data — two common formats:
        // DXSpider tabular (sh/wwv):
        //   14-Dec-2025   15   122  14   1 No Storms -> No Storms   <W0MU>
        // WWV bulletin style (sh/wwv on many clusters):
        //   WWV de W0MU <18Z> :   SFI=122, A=14, K=1, No Storms -> No Storms
        {
            int sfi = 0, aIndex = 0, kIndex = 0;
            bool parsed = false;

            // Try tabular format first
            QRegularExpression tabRegex("\\d{1,2}-\\w{3}-\\d{4}\\s+\\d+\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s");
            QRegularExpressionMatch tabMatch = tabRegex.match(line);
            if (tabMatch.hasMatch()) {
                sfi    = tabMatch.captured(1).toInt();
                aIndex = tabMatch.captured(2).toInt();
                kIndex = tabMatch.captured(3).toInt();
                parsed = true;
            }

            // Try key=value bulletin format (WWV de ... SFI=NNN, A=NN, K=N)
            if (!parsed && line.contains("SFI=", Qt::CaseInsensitive)) {
                QRegularExpression kvRegex("SFI=(\\d+)[^A-Z]*A=(\\d+)[^K]*K=(\\d+)",
                                           QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch kvMatch = kvRegex.match(line);
                if (kvMatch.hasMatch()) {
                    sfi    = kvMatch.captured(1).toInt();
                    aIndex = kvMatch.captured(2).toInt();
                    kIndex = kvMatch.captured(3).toInt();
                    parsed = true;
                }
            }

            if (parsed) {
                DebugLogger::instance().log("DxCluster",
                    QString("Propagation data: SFI=%1 A=%2 K=%3").arg(sfi).arg(aIndex).arg(kIndex));
                emit propagationDataReceived(sfi, aIndex, kIndex);
            }
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
    
    QString band = BandPlan::freq2Band(frequency); // frequency is in kHz
    QString mode = BandPlan::freq2Mode(frequency / 1000.0); // Convert kHz to MHz
    QString modeCat = modeCategory(mode, comment);

    QTableWidgetItem *timeItem = createItem(QTime::currentTime().toString("HH:mm:ss"));
    timeItem->setData(Qt::UserRole,     band);     // band for band filter
    timeItem->setData(Qt::UserRole + 1, modeCat);  // category for mode filter
    m_spotTable->setItem(row, 0, timeItem);
    m_spotTable->setItem(row, 1, createItem(callsign));
    m_spotTable->setItem(row, 2, createItem(QString::number(frequency, 'f', 1)));
    m_spotTable->setItem(row, 3, createItem(mode));
    m_spotTable->setItem(row, 4, createItem(spotter));
    m_spotTable->setItem(row, 5, createItem(comment));

    // Store frequency and mode in the row for click handling
    m_spotTable->item(row, 2)->setData(Qt::UserRole, frequency);
    m_spotTable->item(row, 3)->setData(Qt::UserRole, mode);

    // Apply both filters to this new row
    applyRowFilter(row);
    
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

    // Emit SpotData for band map consumption. frequency here is in kHz.
    SpotData spot;
    spot.callsign  = callsign.trimmed().toUpper();
    spot.freqMhz   = frequency / 1000.0; // kHz → MHz
    spot.mode      = mode;
    spot.spotter   = spotter.trimmed().toUpper();
    spot.timestamp = QDateTime::currentDateTimeUtc();
    spot.status    = ContactStatus::Unknown; // resolved by MainWindow
    emit spotReceived(spot);
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

void DxClusterPanel::onClusterSelectionChanged(const QString& text)
{
    if (m_isConnected && text.trimmed() != m_connectedServer) {
        DebugLogger::instance().log("DxCluster", "Server selection changed while connected — disconnecting");
        onDisconnect();
    }
    // Persist the current selection immediately so a crash doesn't lose it
    Settings& settings = Settings::instance();
    settings.setDxClusterServer(text.trimmed());
    settings.save();
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
    settings.save();
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

void DxClusterPanel::setBands(const QStringList& bands)
{
    if (!m_bandFilterCombo) return;

    QString current = m_bandFilterCombo->currentText();
    m_bandFilterCombo->blockSignals(true);
    m_bandFilterCombo->clear();
    m_bandFilterCombo->addItem("ALL");
    for (const QString& band : bands)
        m_bandFilterCombo->addItem(band);

    // Restore previous selection if still valid, otherwise fall back to ALL
    int idx = m_bandFilterCombo->findText(current);
    m_bandFilterCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_bandFilterCombo->blockSignals(false);

    // Re-apply filter with the (possibly restored) selection
    onBandFilterChanged(m_bandFilterCombo->currentText());
}

// Map raw mode string + comment text → filter category
QString DxClusterPanel::modeCategory(const QString& mode, const QString& comment)
{
    // Check comment first — clusters often annotate FT8/RTTY explicitly
    QString upperComment = comment.toUpper();
    if (upperComment.contains("FT8") || upperComment.contains("FT4") || upperComment.contains("FT2"))
        return "FTx";
    if (upperComment.contains("RTTY"))
        return "RTTY";

    QString upperMode = mode.toUpper();
    if (upperMode == "CW" || upperMode == "CWR")
        return "CW";
    if (upperMode == "USB" || upperMode == "LSB" || upperMode == "SSB" ||
        upperMode == "AM"  || upperMode == "FM"  || upperMode == "PHONE")
        return "SSB";
    if (upperMode == "RTTY")
        return "RTTY";
    if (upperMode == "DIG" || upperMode == "DATA" || upperMode == "FT8" ||
        upperMode == "FT4" || upperMode == "FT2")
        return "FTx";

    return "OTHER";
}

void DxClusterPanel::applyRowFilter(int row)
{
    QTableWidgetItem *timeItem = m_spotTable->item(row, 0);
    if (!timeItem) return;

    QString rowBand    = timeItem->data(Qt::UserRole).toString();
    QString rowModeCat = timeItem->data(Qt::UserRole + 1).toString();

    QString filterBand = m_bandFilterCombo ? m_bandFilterCombo->currentText() : "ALL";
    QString filterMode = m_modeFilterCombo ? m_modeFilterCombo->currentText() : "ALL";

    bool hidden = (filterBand != "ALL" && rowBand != filterBand)
               || (filterMode != "ALL" && rowModeCat != filterMode);
    m_spotTable->setRowHidden(row, hidden);
}

void DxClusterPanel::onBandFilterChanged(const QString& /*band*/)
{
    for (int row = 0; row < m_spotTable->rowCount(); ++row)
        applyRowFilter(row);
}

void DxClusterPanel::onModeFilterChanged(const QString& /*mode*/)
{
    for (int row = 0; row < m_spotTable->rowCount(); ++row)
        applyRowFilter(row);
}

void DxClusterPanel::setSpotCommand(const QString& callsign, double freqKhz)
{
    // Format: dx <freq_in_khz> <callsign>
    QString command = QString("dx %1 %2").arg(freqKhz, 0, 'f', 1).arg(callsign);
    m_commandEdit->setText(command);
    m_commandEdit->setFocus();

    DebugLogger::instance().log("DxCluster", QString("Spot command prepared: %1").arg(command));
}
