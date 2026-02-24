/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "multiplierWidget.h"
#include "debugLogger.h"
#include <QVBoxLayout>

MultiplierWidget::MultiplierWidget(QWidget *parent)
    : QWidget(parent)
    , m_filterCombo(new QComboBox(this))
    , m_workedLabel(new QLabel("Worked: 0/0", this))
    , m_scrollArea(new QScrollArea(this))
    , m_gridContainer(new QWidget)
    , m_gridLayout(new QGridLayout(m_gridContainer))
    , m_multiplierType("multsOnce")
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_filterCombo->hide();  // Hidden by default (shown for per-band/per-mode types)
    layout->addWidget(m_filterCombo);
    layout->addWidget(m_workedLabel);

    m_gridLayout->setSpacing(2);
    m_gridLayout->setContentsMargins(2, 2, 2, 2);
    m_gridContainer->setLayout(m_gridLayout);

    m_scrollArea->setWidget(m_gridContainer);
    m_scrollArea->setWidgetResizable(true);
    layout->addWidget(m_scrollArea);

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MultiplierWidget::onFilterChanged);
}

void MultiplierWidget::setMultiplierList(const QStringList& mults)
{
    DebugLogger::instance().log("MultiplierWidget",
        QString("setMultiplierList called: %1 mults").arg(mults.size()));
    if (!mults.isEmpty()) {
        DebugLogger::instance().log("MultiplierWidget",
            QString("  first: %1, last: %2").arg(mults.first(), mults.last()));
    }
    m_allMults = mults;
    rebuildGrid();
}

void MultiplierWidget::setMultiplierType(const QString& type)
{
    DebugLogger::instance().log("MultiplierWidget",
        QString("setMultiplierType: %1").arg(type));
    m_multiplierType = type;

    // Show filter combo for per-band/per-mode types
    bool showFilter = (type == "multsPerBand" || type == "multsPerMode" || type == "multsPerBandAndMode");
    m_filterCombo->setVisible(showFilter);
}

void MultiplierWidget::setFilterOptions(const QStringList& bands, const QStringList& modes)
{
    m_bands = bands;
    m_modes = modes;

    m_filterCombo->blockSignals(true);
    m_filterCombo->clear();
    m_filterCombo->addItem("All");

    if (m_multiplierType == "multsPerBand") {
        for (const QString& band : bands) {
            m_filterCombo->addItem(band);
        }
    } else if (m_multiplierType == "multsPerMode") {
        for (const QString& mode : modes) {
            m_filterCombo->addItem(mode);
        }
    } else if (m_multiplierType == "multsPerBandAndMode") {
        for (const QString& band : bands) {
            for (const QString& mode : modes) {
                m_filterCombo->addItem(QString("%1 %2").arg(band, mode));
            }
        }
    }

    m_filterCombo->blockSignals(false);
}

void MultiplierWidget::updateWorkedMultipliers(const QSet<QString>& workedSet)
{
    DebugLogger::instance().log("MultiplierWidget",
        QString("updateWorkedMultipliers: %1 worked entries").arg(workedSet.size()));
    m_workedSet = workedSet;
    updateCheckboxStates();
}

void MultiplierWidget::clear()
{
    m_allMults.clear();
    m_workedSet.clear();
    m_checkboxes.clear();

    // Clear grid
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    m_workedLabel->setText("Worked: 0/0");
    m_workedLabel->show();
    m_filterCombo->hide();
}

void MultiplierWidget::onFilterChanged(int /*index*/)
{
    updateCheckboxStates();
}

void MultiplierWidget::rebuildGrid()
{
    DebugLogger::instance().log("MultiplierWidget",
        QString("rebuildGrid: %1 mults, type=%2").arg(m_allMults.size()).arg(m_multiplierType));

    // Clear existing checkboxes
    m_checkboxes.clear();
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (m_allMults.isEmpty()) {
        auto *msg = new QLabel("No Named Multipliers\nFor Station Class", m_gridContainer);
        msg->setAlignment(Qt::AlignCenter);
        msg->setWordWrap(true);
        msg->setStyleSheet("color: gray; font-style: italic;");
        m_gridLayout->addWidget(msg, 0, 0);
        m_workedLabel->hide();
        return;
    }

    m_workedLabel->show();

    // Create checkboxes for each multiplier
    for (int i = 0; i < m_allMults.size(); ++i) {
        auto *cb = new QCheckBox(m_allMults[i], m_gridContainer);
        cb->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cb->setFocusPolicy(Qt::NoFocus);
        cb->setStyleSheet(
            "QCheckBox::indicator:checked { background-color: red; border: 1px solid darkred; }"
        );
        m_gridLayout->addWidget(cb, i / COLUMNS, i % COLUMNS);
        m_checkboxes.append(cb);
    }

    updateCheckboxStates();
}

void MultiplierWidget::updateCheckboxStates()
{
    if (m_allMults.isEmpty()) return;

    QString filter = m_filterCombo->currentText();
    int workedCount = 0;

    for (int i = 0; i < m_allMults.size(); ++i) {
        if (i >= m_checkboxes.size()) break;

        const QString& mult = m_allMults[i];
        bool worked = false;

        if (m_multiplierType == "multsOnce") {
            // Simple: check if mult name is in the worked set
            worked = m_workedSet.contains(mult);
        } else if (filter == "All") {
            // "All" filter: worked if ANY key containing this mult exists
            for (const QString& key : m_workedSet) {
                if (key.startsWith(mult + "_")) {
                    worked = true;
                    break;
                }
            }
        } else if (m_multiplierType == "multsPerBand") {
            // Check for "MULT_band" key
            worked = m_workedSet.contains(QString("%1_%2").arg(mult, filter));
        } else if (m_multiplierType == "multsPerMode") {
            // Check for "MULT_mode" key
            worked = m_workedSet.contains(QString("%1_%2").arg(mult, filter));
        } else if (m_multiplierType == "multsPerBandAndMode") {
            // Filter is "band mode", split and check "MULT_band_mode"
            QStringList parts = filter.split(' ');
            if (parts.size() == 2) {
                worked = m_workedSet.contains(QString("%1_%2_%3").arg(mult, parts[0], parts[1]));
            }
        }

        m_checkboxes[i]->setChecked(worked);
        if (worked) workedCount++;
    }

    m_workedLabel->setText(QString("Worked: %1/%2").arg(workedCount).arg(m_allMults.size()));
}
