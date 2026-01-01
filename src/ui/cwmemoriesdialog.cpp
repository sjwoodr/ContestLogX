/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "cwmemoriesdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>

CwMemoriesDialog::CwMemoriesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("CW Memories Editor");
    setMinimumWidth(600);
    setupUi();
}

CwMemoriesDialog::~CwMemoriesDialog()
{
}

void CwMemoriesDialog::setupUi()
{
    setFixedSize(700, 350);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    
    // Create grid layout for compact display
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(10);
    gridLayout->setVerticalSpacing(8);
    
    // Headers
    QLabel *keyHeader = new QLabel("<b>Key</b>");
    QLabel *titleHeader = new QLabel("<b>Title</b>");
    QLabel *messageHeader = new QLabel("<b>Message</b>");
    gridLayout->addWidget(keyHeader, 0, 0);
    gridLayout->addWidget(titleHeader, 0, 1);
    gridLayout->addWidget(messageHeader, 0, 2);
    
    // Create F1-F8 memory editors in compact grid
    for (int i = 0; i < 8; i++) {
        QLabel *keyLabel = new QLabel(QString("F%1").arg(i + 1));
        keyLabel->setMinimumWidth(30);
        gridLayout->addWidget(keyLabel, i + 1, 0);
        
        m_abbrevEdits[i] = new QLineEdit(this);
        m_abbrevEdits[i]->setMaxLength(5);
        m_abbrevEdits[i]->setPlaceholderText("Title");
        m_abbrevEdits[i]->setFixedWidth(80);
        gridLayout->addWidget(m_abbrevEdits[i], i + 1, 1);
        
        m_textEdits[i] = new QTextEdit(this);
        m_textEdits[i]->setMaximumHeight(28);
        m_textEdits[i]->setPlaceholderText("CW message text...");
        gridLayout->addWidget(m_textEdits[i], i + 1, 2);
    }
    
    gridLayout->setColumnStretch(2, 1); // Make message column stretch
    mainLayout->addLayout(gridLayout);
    
    mainLayout->addStretch();
    
    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CwMemoriesDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CwMemoriesDialog::onCancel);
    mainLayout->addWidget(buttonBox);
}

void CwMemoriesDialog::onSave()
{
    accept();
}

void CwMemoriesDialog::onCancel()
{
    reject();
}

QList<CwMemory> CwMemoriesDialog::getMemories() const
{
    QList<CwMemory> memories;
    for (int i = 0; i < 8; i++) {
        CwMemory mem;
        mem.abbreviation = m_abbrevEdits[i]->text().trimmed();
        mem.text = m_textEdits[i]->toPlainText().trimmed();
        memories.append(mem);
    }
    return memories;
}

void CwMemoriesDialog::setMemories(const QList<CwMemory>& memories)
{
    for (int i = 0; i < qMin(8, memories.size()); i++) {
        m_abbrevEdits[i]->setText(memories[i].abbreviation);
        m_textEdits[i]->setPlainText(memories[i].text);
    }
}
