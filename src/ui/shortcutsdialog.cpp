/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "shortcutsdialog.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QMessageBox>

const QList<ShortcutsDialog::DefaultShortcut> ShortcutsDialog::DEFAULT_SHORTCUTS = {
    {"clearQsoEntry", "Clear QSO Entry Panel", "Ctrl+W"}
};

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Keyboard Shortcuts");
    setMinimumSize(500, 400);
    
    setupUI();
    loadShortcuts();
    populateTable();
}

void ShortcutsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QLabel* label = new QLabel("Configure keyboard shortcuts:");
    mainLayout->addWidget(label);
    
    // Table
    m_table = new QTableWidget;
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Action", "Description", "Key Sequence"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_table);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_addButton = new QPushButton("Add");
    m_editButton = new QPushButton("Edit");
    m_removeButton = new QPushButton("Remove");
    m_resetButton = new QPushButton("Reset to Defaults");
    
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    
    // Dialog buttons
    QHBoxLayout* dialogLayout = new QHBoxLayout;
    m_okButton = new QPushButton("OK");
    m_cancelButton = new QPushButton("Cancel");
    
    dialogLayout->addStretch();
    dialogLayout->addWidget(m_okButton);
    dialogLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(dialogLayout);
    
    // Connect signals
    connect(m_addButton, &QPushButton::clicked, this, &ShortcutsDialog::onAddShortcut);
    connect(m_editButton, &QPushButton::clicked, this, &ShortcutsDialog::onEditShortcut);
    connect(m_removeButton, &QPushButton::clicked, this, &ShortcutsDialog::onRemoveShortcut);
    connect(m_resetButton, &QPushButton::clicked, this, &ShortcutsDialog::onResetDefaults);
    connect(m_okButton, &QPushButton::clicked, this, &ShortcutsDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ShortcutsDialog::loadShortcuts()
{
    Settings& settings = Settings::instance();
    m_shortcuts = settings.getShortcuts();
    
    // Ensure all default shortcuts exist
    for (const auto& def : DEFAULT_SHORTCUTS) {
        if (!m_shortcuts.contains(def.action)) {
            m_shortcuts[def.action] = def.defaultKey;
        }
    }
}

void ShortcutsDialog::populateTable()
{
    m_table->setRowCount(0);
    
    // First, add all defaults and any custom shortcuts
    QMap<QString, QString> allShortcuts = m_shortcuts;
    
    int row = 0;
    for (const auto& def : DEFAULT_SHORTCUTS) {
        m_table->insertRow(row);
        
        QTableWidgetItem* actionItem = new QTableWidgetItem(def.action);
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, actionItem);
        
        QTableWidgetItem* descItem = new QTableWidgetItem(def.description);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, descItem);
        
        QString keySeq = allShortcuts[def.action];
        QTableWidgetItem* keyItem = new QTableWidgetItem(keySeq);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 2, keyItem);
        
        row++;
    }
    
    m_table->resizeColumnsToContents();
}

void ShortcutsDialog::onEditShortcut()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    
    QString action = m_table->item(row, 0)->text();
    QString description = m_table->item(row, 1)->text();
    QString currentKey = m_table->item(row, 2)->text();
    
    bool ok;
    QString newKey = QInputDialog::getText(this,
        "Edit Shortcut",
        QString("Enter key sequence for '%1':").arg(description),
        QLineEdit::Normal,
        currentKey,
        &ok);
    
    if (ok && !newKey.isEmpty()) {
        QKeySequence seq(newKey);
        if (seq.isEmpty()) {
            QMessageBox::warning(this, "Invalid Shortcut", "The key sequence is invalid.");
            return;
        }
        
        m_shortcuts[action] = seq.toString();
        m_table->item(row, 2)->setText(seq.toString());
    }
}

void ShortcutsDialog::onAddShortcut()
{
    // For now, users can only edit defaults
    QMessageBox::information(this, "Custom Shortcuts", "Custom shortcuts can be added in a future version.");
}

void ShortcutsDialog::onRemoveShortcut()
{
    QMessageBox::information(this, "Remove Shortcut", "Default shortcuts cannot be removed.");
}

void ShortcutsDialog::onResetDefaults()
{
    if (QMessageBox::question(this, "Reset Shortcuts",
        "Reset all shortcuts to their default values?",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        m_shortcuts.clear();
        for (const auto& def : DEFAULT_SHORTCUTS) {
            m_shortcuts[def.action] = def.defaultKey;
        }
        populateTable();
    }
}

void ShortcutsDialog::onAccepted()
{
    Settings& settings = Settings::instance();
    settings.setShortcuts(m_shortcuts);
    accept();
}

QMap<QString, QString> ShortcutsDialog::getShortcuts() const
{
    return m_shortcuts;
}
