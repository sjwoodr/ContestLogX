/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "callhistorydialog.h"
#include "callhistory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMenu>
#include <QMessageBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonObject>

CallHistoryDialog::CallHistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Manage Call History");
    setGeometry(100, 100, 800, 600);
    
    setupUI();
    refreshTable();
}

void CallHistoryDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Table for call history
    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(0);
    m_historyTable->setRowCount(0);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    
    connect(m_historyTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item) {
        onTableItemDoubleClicked(item->row(), item->column());
    });
    connect(m_historyTable, &QTableWidget::customContextMenuRequested, this, 
            &CallHistoryDialog::onTableCustomContextMenu);
    
    mainLayout->addWidget(m_historyTable);
    
    // Settings group
    QGroupBox* settingsGroup = new QGroupBox("Settings", this);
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsGroup);
    
    m_enableCheckBox = new QCheckBox("Enable call history insertion during contest", this);
    m_enableCheckBox->setChecked(CallHistory::instance().isEnabled());
    connect(m_enableCheckBox, &QCheckBox::toggled, this, &CallHistoryDialog::onEnableToggled);
    settingsLayout->addWidget(m_enableCheckBox);
    
    m_autoSaveCheckBox = new QCheckBox("Auto-save call history when saving log", this);
    m_autoSaveCheckBox->setChecked(CallHistory::instance().isAutoSaveEnabled());
    connect(m_autoSaveCheckBox, &QCheckBox::toggled, this, &CallHistoryDialog::onAutoSaveToggled);
    settingsLayout->addWidget(m_autoSaveCheckBox);
    
    mainLayout->addWidget(settingsGroup);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_clearButton = new QPushButton("Clear All History", this);
    connect(m_clearButton, &QPushButton::clicked, this, &CallHistoryDialog::onClearHistory);
    buttonLayout->addWidget(m_clearButton);
    
    QPushButton* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
}

void CallHistoryDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshTable();
}

void CallHistoryDialog::refreshTable()
{
    m_historyTable->setRowCount(0);
    
    // Get all unique field names
    QStringList fieldNames = CallHistory::instance().getAllFieldNames();
    
    m_historyTable->setColumnCount(fieldNames.size());
    m_historyTable->setHorizontalHeaderLabels(fieldNames);
    
    // Get all records
    QJsonArray records = CallHistory::instance().getAllRecords();
    m_historyTable->setRowCount(records.size());
    
    for (int row = 0; row < records.size(); ++row) {
        QJsonObject record = records[row].toObject();
        
        for (int col = 0; col < fieldNames.size(); ++col) {
            QString fieldName = fieldNames[col];
            QString value = record[fieldName].toString("");
            
            QTableWidgetItem* item = new QTableWidgetItem(value);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_historyTable->setItem(row, col, item);
        }
    }
    
    // Resize columns to content
    m_historyTable->resizeColumnsToContents();
}

void CallHistoryDialog::onTableItemDoubleClicked(int row, int column)
{
    editRecord(row);
}

void CallHistoryDialog::onTableCustomContextMenu(const QPoint &pos)
{
    QTableWidgetItem* item = m_historyTable->itemAt(pos);
    if (!item) return;
    
    m_contextMenuRow = item->row();
    
    QMenu contextMenu;
    contextMenu.addAction("Edit", this, &CallHistoryDialog::onEditRecord);
    contextMenu.addAction("Delete", this, &CallHistoryDialog::onDeleteRecord);
    contextMenu.exec(m_historyTable->mapToGlobal(pos));
}

void CallHistoryDialog::onEditRecord()
{
    if (m_contextMenuRow >= 0) {
        editRecord(m_contextMenuRow);
    }
}

void CallHistoryDialog::onDeleteRecord()
{
    if (m_contextMenuRow < 0) return;
    
    QTableWidgetItem* callItem = m_historyTable->item(m_contextMenuRow, 0);
    if (!callItem) return;
    
    QString callsign = callItem->text();
    
    if (QMessageBox::question(this, "Confirm Delete",
            QString("Delete call history for %1?").arg(callsign)) == QMessageBox::Yes) {
        CallHistory::instance().deleteRecord(callsign);
        CallHistory::instance().save();
        refreshTable();
    }
}

void CallHistoryDialog::editRecord(int row)
{
    if (row < 0) return;
    
    // Get the callsign (first column)
    QTableWidgetItem* callItem = m_historyTable->item(row, 0);
    if (!callItem) return;
    
    QString callsign = callItem->text();
    QStringList fieldNames = CallHistory::instance().getAllFieldNames();
    
    // Get current record
    QJsonObject record;
    QJsonArray records = CallHistory::instance().getAllRecords();
    for (const QJsonValue& val : records) {
        if (val.isObject() && val.toObject()["CALL"].toString() == callsign) {
            record = val.toObject();
            break;
        }
    }
    
    // Create edit dialog
    QDialog editDialog(this);
    editDialog.setWindowTitle(QString("Edit %1").arg(callsign));
    editDialog.setGeometry(200, 200, 500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&editDialog);
    QMap<QString, QLineEdit*> fieldEdits;
    
    // Add CALL field as read-only
    QHBoxLayout* callLayout = new QHBoxLayout();
    QLabel* callLabel = new QLabel("CALL:", &editDialog);
    callLabel->setMinimumWidth(80);
    QLineEdit* callEdit = new QLineEdit(&editDialog);
    callEdit->setText(callsign);
    callEdit->setReadOnly(true);
    callLayout->addWidget(callLabel);
    callLayout->addWidget(callEdit);
    layout->addLayout(callLayout);
    
    // Add all other fields
    for (int i = 1; i < fieldNames.size(); ++i) {
        QString fieldName = fieldNames[i];
        QString fieldValue = record[fieldName].toString("");
        
        QHBoxLayout* fieldLayout = new QHBoxLayout();
        QLabel* label = new QLabel(fieldName + ":", &editDialog);
        label->setMinimumWidth(80);
        
        QLineEdit* edit = new QLineEdit(&editDialog);
        edit->setText(fieldValue);
        fieldEdits[fieldName] = edit;
        
        fieldLayout->addWidget(label);
        fieldLayout->addWidget(edit);
        layout->addLayout(fieldLayout);
    }
    
    layout->addStretch();
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* saveBtn = new QPushButton("Save", &editDialog);
    QPushButton* cancelBtn = new QPushButton("Cancel", &editDialog);
    
    connect(saveBtn, &QPushButton::clicked, &editDialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &editDialog, &QDialog::reject);
    
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);
    
    if (editDialog.exec() == QDialog::Accepted) {
        // Collect updated fields
        QMap<QString, QString> updatedFields;
        
        for (auto it = fieldEdits.begin(); it != fieldEdits.end(); ++it) {
            QString value = it.value()->text().trimmed();
            if (!value.isEmpty()) {
                updatedFields[it.key()] = value;
            }
        }
        
        // Update the record
        if (!updatedFields.isEmpty()) {
            CallHistory::instance().addOrUpdateRecord(callsign, updatedFields);
            CallHistory::instance().save();
            refreshTable();
        }
    }
}

void CallHistoryDialog::onEnableToggled(bool checked)
{
    CallHistory::instance().setEnabled(checked);
}

void CallHistoryDialog::onAutoSaveToggled(bool checked)
{
    CallHistory::instance().setAutoSaveEnabled(checked);
}

void CallHistoryDialog::onClearHistory()
{
    if (QMessageBox::question(this, "Confirm Clear",
            "Clear all call history? This cannot be undone.") == QMessageBox::Yes) {
        CallHistory::instance().clear();
        refreshTable();
    }
}
