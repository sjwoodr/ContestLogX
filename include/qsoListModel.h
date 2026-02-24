/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
    
    // Utility
    int count() const { return m_qsos.count(); }
    const QList<QsoRecord>& getQsos() const { return m_qsos; }
    QList<QsoRecord> getAllQsos() const { return m_qsos; }

signals:
    void qsoAdded(int row);
    void qsoRemoved(int row);
    void qsoUpdated(int row);

private:
    QList<QsoRecord> m_qsos;
    QStringList m_columnHeaders;
    
    // Default column headers if none set
    QStringList defaultHeaders() const;
};

#endif // QSOLISTMODEL_H
