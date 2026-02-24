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

#ifndef MULTIPLIERWIDGET_H
#define MULTIPLIERWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <QSet>
#include <QStringList>

class MultiplierWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MultiplierWidget(QWidget *parent = nullptr);

    void setMultiplierList(const QStringList& mults);
    void setMultiplierType(const QString& type);
    void setFilterOptions(const QStringList& bands, const QStringList& modes);
    void updateWorkedMultipliers(const QSet<QString>& workedSet);
    void clear();

private slots:
    void onFilterChanged(int index);

private:
    void rebuildGrid();
    void updateCheckboxStates();

    QComboBox *m_filterCombo;
    QLabel *m_workedLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_gridContainer;
    QGridLayout *m_gridLayout;

    QStringList m_allMults;          // Ordered list of all valid multipliers
    QSet<QString> m_workedSet;       // Current worked set (keys depend on mult type)
    QString m_multiplierType;        // multsOnce, multsPerBand, etc.
    QStringList m_bands;
    QStringList m_modes;
    QList<QCheckBox*> m_checkboxes;

    static const int COLUMNS = 6;
};

#endif // MULTIPLIERWIDGET_H
