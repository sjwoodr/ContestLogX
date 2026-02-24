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

#ifndef QSOLISTMODEL_H
#define QSOLISTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>
#include "qsoRecord.h"

/**
 * @brief Model for displaying QSO records in a table view
 * 
 * Implements Qt's Model/View pattern for QSO log display with dynamic columns
 */
class QsoListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit QsoListModel(QObject *parent = nullptr);
    
    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    
    // Data manipulation
    void addQso(const QsoRecord& qso);
    void removeQso(int row);
    void updateQso(int row, const QsoRecord& qso);
    void updateMultiplierCount(int row, int multCount);
    void updateDxccCount(int row, int dxccCount);
    void updateItuRegionCount(int row, int ituRegionCount);
    void updateGridSquareMultiplier(int row, const QString& gridSquare);
    void updateGridSquareMultiplierCount(int row, int gridSquareMultCount);
    QsoRecord getQso(int row) const;
    void clear();
    void replaceAll(const QList<QsoRecord>& qsos);
    void reverseQsos();
    
    // Column configuration
    void setColumnHeaders(const QStringList& headers);
    QStringList columnHeaders() const { return m_columnHeaders; }
    
    // Filtering
    void setFilter(const QString& text);
    QString filterText() const { return m_filterText; }

    // Utility
    int count() const { return m_qsos.count(); }        // always total (unfiltered)
    const QList<QsoRecord>& getQsos() const { return m_qsos; }
    QList<QsoRecord> getAllQsos() const { return m_qsos; }

signals:
    void qsoAdded(int row);
    void qsoRemoved(int row);
    void qsoUpdated(int row);

private:
    QList<QsoRecord> m_qsos;
    QStringList m_columnHeaders;
    QString m_filterText;
    QList<int> m_visibleIndices;  // source-row indices visible under current filter; empty = no filter

    int mapToSource(int viewRow) const;   // view row → m_qsos index
    int mapFromSource(int sourceRow) const; // m_qsos index → view row (-1 if filtered out)
    void rebuildVisibleIndices();
    bool matchesFilter(const QsoRecord& qso) const;

    QStringList defaultHeaders() const;
};

#endif // QSOLISTMODEL_H
