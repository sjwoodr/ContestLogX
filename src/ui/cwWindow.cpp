/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "cwWindow.h"
#include "rigInterface.h"
#include "debugLogger.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QDebug>
#include <QKeyEvent>

CWWindow::CWWindow(RigInterface* rigClient, QWidget *parent)
    : QWidget(parent), rigClient(rigClient)
{
    DebugLogger::instance().log("CWWindow", "========== CW Window Constructor Started ==========");
    DebugLogger::instance().log("CWWindow", QString("RigClient pointer: %1").arg(rigClient ? "VALID" : "NULL"));
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(5);
    
    // History text (read-only, multi-line)
    historyText = new QTextEdit(this);
    historyText->setReadOnly(true);
    historyText->setMaximumHeight(80);
    historyText->setStyleSheet("QTextEdit { font-family: monospace; font-size: 10pt; }");
    mainLayout->addWidget(historyText);
    
    // Input line with controls
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(5);
    
    inputLine = new QLineEdit(this);
    inputLine->setPlaceholderText("Enter CW text and press Enter to send...");
    inputLine->setFocus();
    inputLine->installEventFilter(this);
    installEventFilter(this);
    inputLayout->addWidget(inputLine);
    
    // WPM label and spinbox
    QLabel* wpmLabel = new QLabel("WPM:", this);
    inputLayout->addWidget(wpmLabel);
    
    wpmSpinBox = new QSpinBox(this);
    wpmSpinBox->setRange(5, 60);
    
    // Load saved WPM from settings
    Settings& settings = Settings::instance();
    int savedWpm = settings.getCwWpm();
    wpmSpinBox->setValue(savedWpm);
    
    wpmSpinBox->setFixedWidth(60);
    inputLayout->addWidget(wpmSpinBox);
    
    // Halt button
    haltButton = new QPushButton("Halt", this);
    haltButton->setFixedWidth(60);
    inputLayout->addWidget(haltButton);
    
    // Clear button
    clearButton = new QPushButton("Clear", this);
    clearButton->setFixedWidth(60);
    inputLayout->addWidget(clearButton);
    
    mainLayout->addLayout(inputLayout);
    
    // F-key memory buttons at bottom (single row)
    QHBoxLayout* memoryLayout = new QHBoxLayout();
    memoryLayout->setSpacing(3);
    
    for (int i = 0; i < 8; i++) {
        memoryButtons[i] = new QPushButton(QString("F%1\n---").arg(i + 1), this);
        memoryButtons[i]->setMaximumHeight(45);
        memoryButtons[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        memoryButtons[i]->setStyleSheet("QPushButton { font-size: 9pt; padding: 2px; }");
        memoryButtons[i]->setFocusPolicy(Qt::NoFocus);
        connect(memoryButtons[i], &QPushButton::clicked, this, [this, i]() { onMemoryButton(i); });
        memoryLayout->addWidget(memoryButtons[i]);
    }
    mainLayout->addLayout(memoryLayout);

    // Push content to the top; excess dock height becomes empty space at bottom
    // rather than inflating the history area or inter-widget gaps.
    mainLayout->addStretch();

    // Connections
    DebugLogger::instance().log("CWWindow", "Setting up signal connections...");
    bool connResult1 = connect(inputLine, &QLineEdit::returnPressed, this, &CWWindow::onSendCW);
    bool connResult2 = connect(clearButton, &QPushButton::clicked, this, &CWWindow::onClear);
    bool connResult3 = connect(haltButton, &QPushButton::clicked, this, &CWWindow::onHalt);
    bool connResult4 = connect(wpmSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &CWWindow::onWpmChanged);
    
    DebugLogger::instance().log("CWWindow", QString("CW Window connections: returnPressed=%1, clear=%2, halt=%3, wpm=%4")
        .arg(connResult1 ? "SUCCESS" : "FAILED")
        .arg(connResult2 ? "SUCCESS" : "FAILED")
        .arg(connResult3 ? "SUCCESS" : "FAILED")
        .arg(connResult4 ? "SUCCESS" : "FAILED"));
    
    // Get initial WPM from rig if connected and clear any pending CW buffer
    if (rigClient && rigClient->isConnected()) {
        rigClient->stopCW();
        DebugLogger::instance().log("CWWindow", "Cleared CW buffer on window open");
        
        int currentWpm = rigClient->getCWSpeed();
        if (currentWpm > 0) {
            wpmSpinBox->setValue(currentWpm);
        }
    }
    
    DebugLogger::instance().log("CWWindow", "========== CW Window Constructor Completed ==========");
}

CWWindow::~CWWindow()
{
}

void CWWindow::sendCWText(const QString& text)
{
    if (text.isEmpty()) return;

    // Hamlib/rigctld CW keying depends on rig support — most rigs don't have it
    if (Settings::instance().getRigBackend() == "hamlib") {
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
        historyText->append(QString("<span style='color: orange;'>[%1] CW keying is not available via Hamlib for most rigs. Use flrig for CW keying.</span>").arg(timestamp));
        return;
    }

    if (!rigClient) {
        DebugLogger::instance().log("CWWindow", QString("Cannot send CW - rigClient is NULL: %1").arg(text));
        return;
    }
    if (!rigClient->isConnected()) {
        DebugLogger::instance().log("CWWindow", QString("Cannot send CW - rig not connected: %1").arg(text));
        return;
    }

    int currentWpm = wpmSpinBox->value();
    rigClient->setCWSpeed(currentWpm);

    // Notify CW decoder (via MainWindow) before the send so it can mute its
    // bins for the owning radio — prevents self-decode of ContestLogX's own
    // keying bleeding back through the audio input (SPEC-005 FR-019c).
    emit aboutToSendCw(rigClient, text, currentWpm);

    bool success = rigClient->sendCW(text);

    if (success) {
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
        historyText->append(QString("[%1] %2").arg(timestamp, text));
        DebugLogger::instance().log("CWWindow", QString("Sent CW at %1 WPM: %2").arg(currentWpm).arg(text));
    } else {
        DebugLogger::instance().log("CWWindow", QString("Failed to send CW: %1").arg(text));
    }
}

void CWWindow::onSendCW()
{
    QString text = inputLine->text().trimmed();
    if (text.isEmpty()) return;
    
    inputLine->clear();
    sendCWText(text);
}

void CWWindow::onClear()
{
    historyText->clear();
    inputLine->clear();
}

void CWWindow::onHalt()
{
    // Stop CW by clearing the buffer
    if (rigClient && rigClient->isConnected()) {
        rigClient->stopCW();
        DebugLogger::instance().log("CWWindow", "Halt CW requested - buffer cleared");
    }
    inputLine->clear();
}

void CWWindow::onWpmChanged(int wpm)
{
    // Save WPM to settings
    Settings& settings = Settings::instance();
    settings.setCwWpm(wpm);
    
    // Emit signal so main window can update its display
    emit wpmChanged(wpm);
    
    if (rigClient && rigClient->isConnected()) {
        rigClient->setCWSpeed(wpm);
        DebugLogger::instance().log("CWWindow", QString("WPM changed to %1").arg(wpm));
    }
}

void CWWindow::onMemoryButton(int fKey)
{
    if (fKey >= 0 && fKey < memories.size()) {
        const CwMemory& mem = memories[fKey];
        if (!mem.text.isEmpty()) {
            emit memoryTriggered(fKey, mem.text);
        }
    }
}

void CWWindow::setMemoriesFont(const QFont& font)
{
    QString styleSheet = QString("QPushButton { font-family: '%1'; font-size: %2pt; padding: 2px; }")
        .arg(font.family())
        .arg(font.pointSize());
    for (int i = 0; i < 8; i++) {
        memoryButtons[i]->setStyleSheet(styleSheet);
    }
}

void CWWindow::setMemories(const QList<CwMemory>& mems)
{
    memories = mems;
    for (int i = 0; i < 8; i++) {
        QString label = QString("F%1").arg(i + 1);
        if (i < memories.size() && !memories[i].abbreviation.isEmpty()) {
            label += "\n" + memories[i].abbreviation;
        } else {
            label += "\n---";
        }
        memoryButtons[i]->setText(label);

        // Tooltip shows the full memory text so the operator can see
        // exactly what will be sent without opening the editor. Empty
        // slots get an empty tooltip (Qt suppresses the popup entirely).
        const QString tip = (i < memories.size()) ? memories[i].text : QString();
        memoryButtons[i]->setToolTip(tip);
    }
}

bool CWWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            onHalt();
            return true;
        }

        if (obj == inputLine &&
                (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            onSendCW();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

