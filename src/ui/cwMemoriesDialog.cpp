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

#include "cwMemoriesDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStyle>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>

CwMemoriesDialog::CwMemoriesDialog(QWidget *parent)
    : QDialog(parent)
    , m_stationRadio(nullptr)
    , m_contestRadio(nullptr)
    , m_snPaddingCombo(nullptr)
    , m_snCutNumbersCheck(nullptr)
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
    setFixedSize(700, 460);

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

    connect(m_stationRadio, &QRadioButton::toggled, this, &CwMemoriesDialog::onModeToggled);

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

    // Separator
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // SN options group box
    QGroupBox *snGroup = new QGroupBox("{SN} Serial Number Options", this);
    QHBoxLayout *snLayout = new QHBoxLayout(snGroup);

    snLayout->addWidget(new QLabel("Minimum digits:"));
    m_snPaddingCombo = new QComboBox(this);
    m_snPaddingCombo->addItem("1  (e.g. 7)", 1);
    m_snPaddingCombo->addItem("2  (e.g. 07)", 2);
    m_snPaddingCombo->addItem("3  (e.g. 007)", 3);
    m_snPaddingCombo->setFixedWidth(130);
    snLayout->addWidget(m_snPaddingCombo);

    snLayout->addSpacing(20);

    m_snCutNumbersCheck = new QCheckBox("Cut numbers  (0→T, 9→N, 1→A)", this);
    snLayout->addWidget(m_snCutNumbersCheck);
    snLayout->addStretch();

    mainLayout->addWidget(snGroup);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    if (auto *btn = buttonBox->button(QDialogButtonBox::Save))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    if (auto *btn = buttonBox->button(QDialogButtonBox::Cancel))
        btn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CwMemoriesDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CwMemoriesDialog::onCancel);
    mainLayout->addWidget(buttonBox);
}

void CwMemoriesDialog::onModeToggled()
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

void CwMemoriesDialog::onSave()
{
    accept();
}

void CwMemoriesDialog::onCancel()
{
    reject();
}

void CwMemoriesDialog::loadMemoriesToUi(const QList<CwMemory>& memories)
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

QList<CwMemory> CwMemoriesDialog::getMemoriesFromUi() const
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

QList<CwMemory> CwMemoriesDialog::getMemories() const
{
    // Return whatever is currently displayed (which reflects the active radio)
    return getMemoriesFromUi();
}

void CwMemoriesDialog::setMemories(const QList<CwMemory>& memories)
{
    m_stationMemories = memories;
    if (!m_currentIsContest) {
        loadMemoriesToUi(memories);
    }
}

void CwMemoriesDialog::setContestMemories(const QList<CwMemory>& memories)
{
    m_contestMemories = memories;
    if (m_currentIsContest) {
        loadMemoriesToUi(memories);
    }
}

void CwMemoriesDialog::setContestMode(bool contest)
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

bool CwMemoriesDialog::isContestMode() const
{
    return m_contestRadio->isChecked();
}

int CwMemoriesDialog::getSnPadding() const
{
    return m_snPaddingCombo->currentData().toInt();
}

void CwMemoriesDialog::setSnPadding(int digits)
{
    int idx = m_snPaddingCombo->findData(digits);
    if (idx >= 0)
        m_snPaddingCombo->setCurrentIndex(idx);
}

bool CwMemoriesDialog::getSnCutNumbers() const
{
    return m_snCutNumbersCheck->isChecked();
}

void CwMemoriesDialog::setSnCutNumbers(bool enabled)
{
    m_snCutNumbersCheck->setChecked(enabled);
}
