/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "qsolistmodel.h"
#include "debuglogger.h"
#include <QColor>

QsoListModel::QsoListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_columnHeaders = defaultHeaders();
}

int QsoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_qsos.count();
}

int QsoListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_columnHeaders.count();
}

QStringList QsoListModel::defaultHeaders() const
{
    return QStringList() << "#" << "DATE" << "TIME" << "CALL" << "FREQ" << "MODE" 
                         << "RSTs" << "RSTr" << "EXCHs" << "EXCHr" 
                         << "Nr" << "Dupe" << "M" << "C" << "P" << "COMMENT";
}

QVariant QsoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_qsos.count())
        return QVariant();
    
    const QsoRecord& qso = m_qsos.at(index.row());
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        QString header = m_columnHeaders.at(index.column()).toUpper();
        QString originalHeader = m_columnHeaders.at(index.column());  // Keep original case
        
        // QSO number column - always first column
        if (header == "#") {
            return index.row() + 1;
        }
        // Map header names to data (case-insensitive)
        else if (header == "DATE") {
            return qso.getDateTime().toUTC().toString("yyyy-MM-dd");
        } else if (header == "TIME") {
            return qso.getDateTime().toUTC().toString("HH:mm:ss");
        } else if (header == "CALL") {
            return qso.getCall().toUpper();
        } else if (header == "FREQUENCY" || header == "FREQ") {
            // Format frequency as integer kHz without scientific notation
            bool ok;
            double freq = qso.getFrequency().toDouble(&ok);
            if (ok) {
                return QString::number((long long)freq);
            }
            return qso.getFrequency();
        } else if (header == "MODE") {
            return qso.getMode();
        } else if (header == "RSTS") {
            return qso.getRstSent();
        } else if (header == "RSTR") {
            return qso.getRstReceived();
        } else if (header == "NAMES") {
            return qso.getExchangeField("NAMEs");
        } else if (header == "NAMER") {
            return qso.getExchangeField("NAMEr");
        } else if (header == "EXCHS") {
            return qso.getExchangeSent();
        } else if (header == "EXCHR") {
            return qso.getExchangeReceived();
        } else if (header == "SNS") {
            return qso.getExchangeField("SNs");
        } else if (header == "SNR") {
            return qso.getExchangeField("SNr");
        } else if (header == "SERIAL" || header == "NR") {
            return QVariant::fromValue(qso.getSerial());
        } else if (header == "DUPE") {
            return qso.isDupe() ? "Y" : "";
        } else if (header == "M") {
            return qso.getMultiplierCount();
        } else if (header == "C") {
            return qso.getDxccCount();
        } else if (header == "P" || header == "POINTS") {
            return qso.getPoints();
        } else if (header == "COMMENT") {
            return qso.getComment();
        } else if (header == "GRID_MULT") {
            return qso.getGridSquareMultiplierCount();
        } else {
            // Try as exchange field - use original case-sensitive name
            return qso.getExchangeField(originalHeader);
        }
    }
    else if (role == Qt::ForegroundRole) {
        if (qso.isDupe() || qso.isOutOfBand()) {
            return QColor(139, 0, 0);  // Dark red text
        }
    }
    else if (role == Qt::BackgroundRole) {
        if (qso.isDupe() || qso.isOutOfBand()) {
            return QColor(0, 0, 0);  // Black background
        }
    }
    
    return QVariant();
}

QVariant QsoListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();
    
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_columnHeaders.count()) {
            return m_columnHeaders.at(section);
        }
        return QVariant();
    }
    else {
        return section + 1;  // Row numbers
    }
}

void QsoListModel::setColumnHeaders(const QStringList& headers)
{
    beginResetModel();
    m_columnHeaders = headers;
    endResetModel();
}

void QsoListModel::addQso(const QsoRecord& qso)
{
    int row = m_qsos.count();
    beginInsertRows(QModelIndex(), row, row);
    m_qsos.append(qso);
    endInsertRows();
    emit qsoAdded(row);
}

void QsoListModel::removeQso(int row)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    beginRemoveRows(QModelIndex(), row, row);
    m_qsos.removeAt(row);
    endRemoveRows();
    emit qsoRemoved(row);
}

void QsoListModel::updateQso(int row, const QsoRecord& qso)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row] = qso;
    emit dataChanged(index(row, 0), index(row, m_columnHeaders.count() - 1));
    emit qsoUpdated(row);
}

void QsoListModel::updateMultiplierCount(int row, int multCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row].setMultiplierCount(multCount);
    
    // Find the M column index to update just that cell
    int multColIndex = m_columnHeaders.indexOf("M");
    if (multColIndex >= 0) {
        emit dataChanged(index(row, multColIndex), index(row, multColIndex));
    }
}

void QsoListModel::updateDxccCount(int row, int dxccCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row].setDxccCount(dxccCount);
    
    // Find the C column index to update just that cell
    int dxccColIndex = m_columnHeaders.indexOf("C");
    if (dxccColIndex >= 0) {
        emit dataChanged(index(row, dxccColIndex), index(row, dxccColIndex));
    }
}

void QsoListModel::updateItuRegionCount(int row, int ituRegionCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row].setItuRegionCount(ituRegionCount);
    
    // ITU regions may be displayed in a separate column if the contest uses them
    // For now, we'll look for an "ITU" or similar column
    int ituColIndex = m_columnHeaders.indexOf("ITU");
    if (ituColIndex < 0) {
        ituColIndex = m_columnHeaders.indexOf("Region");
    }
    if (ituColIndex >= 0) {
        emit dataChanged(index(row, ituColIndex), index(row, ituColIndex));
    }
}

void QsoListModel::updateGridSquareMultiplier(int row, const QString& gridSquare)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    DebugLogger::instance().log("QsoListModel", QString("updateGridSquareMultiplier: row=%1, gridSquare='%2'").arg(row).arg(gridSquare));
    m_qsos[row].setGridSquareMultiplier(gridSquare);
    
    // Look for GRID_MULT column
    int gridMultColIndex = m_columnHeaders.indexOf("GRID_MULT");
    DebugLogger::instance().log("QsoListModel", QString("  GRID_MULT column index: %1").arg(gridMultColIndex));
    if (gridMultColIndex >= 0) {
        emit dataChanged(index(row, gridMultColIndex), index(row, gridMultColIndex));
    }
}

void QsoListModel::updateGridSquareMultiplierCount(int row, int gridSquareMultCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row].setGridSquareMultiplierCount(gridSquareMultCount);
    
    // Look for GRID_MULT column
    int gridMultColIndex = m_columnHeaders.indexOf("GRID_MULT");
    if (gridMultColIndex >= 0) {
        emit dataChanged(index(row, gridMultColIndex), index(row, gridMultColIndex));
    }
}

QsoRecord QsoListModel::getQso(int row) const
{
    if (row < 0 || row >= m_qsos.count())
        return QsoRecord();
    return m_qsos.at(row);
}

void QsoListModel::clear()
{
    beginResetModel();
    m_qsos.clear();
    endResetModel();
}

void QsoListModel::reverseQsos()
{
    if (m_qsos.isEmpty()) {
        return;
    }
    
    beginResetModel();
    std::reverse(m_qsos.begin(), m_qsos.end());
    endResetModel();
}
