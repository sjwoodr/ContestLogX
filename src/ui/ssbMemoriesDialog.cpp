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

#include "ssbMemoriesDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QButtonGroup>

SsbMemoriesDialog::SsbMemoriesDialog(QWidget *parent)
    : QDialog(parent)
    , m_stationRadio(nullptr)
    , m_contestRadio(nullptr)
{
    setWindowTitle("SSB Memories Editor");
    setMinimumWidth(600);
    setupUi();
}

SsbMemoriesDialog::~SsbMemoriesDialog()
{
}

void SsbMemoriesDialog::setupUi()
{
    setFixedSize(700, 390);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Memory source radio buttons
    QHBoxLayout *radioLayout = new QHBoxLayout();
    m_stationRadio = new QRadioButton("Station Memories (global)", this);
    m_contestRadio = new QRadioButton("Contest-Specific Memories", this);
    m_stationRadio->setChecked(true);
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(m_stationRadio);
    group->addButton(m_contestRadio);
    radioLayout->addWidget(m_stationRadio);
    radioLayout->addWidget(m_contestRadio);
    radioLayout->addStretch();
    mainLayout->addLayout(radioLayout);

    connect(m_stationRadio, &QRadioButton::toggled, this, &SsbMemoriesDialog::onModeToggled);

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
        m_abbrevEdits[i]->setMaxLength(6);
        m_abbrevEdits[i]->setPlaceholderText("Title");
        m_abbrevEdits[i]->setFixedWidth(80);
        gridLayout->addWidget(m_abbrevEdits[i], i + 1, 1);

        m_textEdits[i] = new QTextEdit(this);
        m_textEdits[i]->setMaximumHeight(28);
        m_textEdits[i]->setPlaceholderText("SSB message text...");
        gridLayout->addWidget(m_textEdits[i], i + 1, 2);
    }

    gridLayout->setColumnStretch(2, 1); // Make message column stretch
    mainLayout->addLayout(gridLayout);

    mainLayout->addStretch();

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SsbMemoriesDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SsbMemoriesDialog::onCancel);
    mainLayout->addWidget(buttonBox);
}

void SsbMemoriesDialog::onModeToggled()
{
    bool switchingToContest = m_contestRadio->isChecked();

    // Save current UI state to the appropriate internal list
    if (m_currentIsContest) {
        m_contestMemories = getMemoriesFromUi();
    } else {
        m_stationMemories = getMemoriesFromUi();
    }

    // Load the other set into the UI
    if (switchingToContest) {
        loadMemoriesToUi(m_contestMemories);
    } else {
        loadMemoriesToUi(m_stationMemories);
    }

    m_currentIsContest = switchingToContest;
}

void SsbMemoriesDialog::onSave()
{
    accept();
}

void SsbMemoriesDialog::onCancel()
{
    reject();
}

void SsbMemoriesDialog::loadMemoriesToUi(const QList<SsbMemory>& memories)
{
    for (int i = 0; i < 8; i++) {
        if (i < memories.size()) {
            m_abbrevEdits[i]->setText(memories[i].abbreviation);
            m_textEdits[i]->setPlainText(memories[i].text);
        } else {
            m_abbrevEdits[i]->clear();
            m_textEdits[i]->clear();
        }
    }
}

QList<SsbMemory> SsbMemoriesDialog::getMemoriesFromUi() const
{
    QList<SsbMemory> memories;
    for (int i = 0; i < 8; i++) {
        SsbMemory mem;
        mem.abbreviation = m_abbrevEdits[i]->text().trimmed();
        mem.text = m_textEdits[i]->toPlainText().trimmed();
        memories.append(mem);
    }
    return memories;
}

QList<SsbMemory> SsbMemoriesDialog::getMemories() const
{
    return getMemoriesFromUi();
}

void SsbMemoriesDialog::setMemories(const QList<SsbMemory>& memories)
{
    m_stationMemories = memories;
    if (!m_currentIsContest) {
        loadMemoriesToUi(memories);
    }
}

void SsbMemoriesDialog::setContestMemories(const QList<SsbMemory>& memories)
{
    m_contestMemories = memories;
    if (m_currentIsContest) {
        loadMemoriesToUi(memories);
    }
}

void SsbMemoriesDialog::setContestMode(bool contest)
{
    // Block signals to prevent onModeToggled from overwriting memories
    m_stationRadio->blockSignals(true);
    m_contestRadio->blockSignals(true);

    m_currentIsContest = contest;
    if (contest) {
        m_contestRadio->setChecked(true);
        loadMemoriesToUi(m_contestMemories);
    } else {
        m_stationRadio->setChecked(true);
        loadMemoriesToUi(m_stationMemories);
    }

    m_stationRadio->blockSignals(false);
    m_contestRadio->blockSignals(false);
}

bool SsbMemoriesDialog::isContestMode() const
{
    return m_contestRadio->isChecked();
}
