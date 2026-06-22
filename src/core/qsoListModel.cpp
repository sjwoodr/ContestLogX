/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "qsoListModel.h"
#include "debugLogger.h"
#include <QColor>
#include <algorithm>

QsoListModel::QsoListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_columnHeaders = defaultHeaders();
}

int QsoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visibleIndices.isEmpty() ? m_qsos.count() : m_visibleIndices.count();
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
    if (!index.isValid() || index.row() >= rowCount())
        return QVariant();

    const QsoRecord& qso = m_qsos.at(mapToSource(index.row()));
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        QString header = m_columnHeaders.at(index.column()).toUpper();
        QString originalHeader = m_columnHeaders.at(index.column());  // Keep original case
        
        // QSO number column - show logged serial so it stays meaningful after sorting
        if (header == "#") {
            unsigned long serial = qso.getSerial();
            return serial > 0 ? QVariant::fromValue(serial) : QVariant(index.row() + 1);
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
    rebuildVisibleIndices();
    endResetModel();
}

void QsoListModel::addQso(const QsoRecord& qso)
{
    int sourceRow = m_qsos.count();
    if (m_visibleIndices.isEmpty()) {
        // No filter active - simple append
        beginInsertRows(QModelIndex(), sourceRow, sourceRow);
        m_qsos.append(qso);
        endInsertRows();
    } else {
        // Filter active - add to source, then expose if it matches
        m_qsos.append(qso);
        if (matchesFilter(qso)) {
            int viewRow = m_visibleIndices.count();
            beginInsertRows(QModelIndex(), viewRow, viewRow);
            m_visibleIndices.append(sourceRow);
            endInsertRows();
        }
    }
    emit qsoAdded(sourceRow);
}

void QsoListModel::removeQso(int viewRow)
{
    if (viewRow < 0 || viewRow >= rowCount())
        return;

    int sourceRow = mapToSource(viewRow);
    beginRemoveRows(QModelIndex(), viewRow, viewRow);
    m_qsos.removeAt(sourceRow);
    if (!m_visibleIndices.isEmpty()) {
        // Adjust remaining visible indices that pointed past the removed source row
        for (int& idx : m_visibleIndices) {
            if (idx > sourceRow) --idx;
        }
        m_visibleIndices.removeAt(viewRow);
    }
    endRemoveRows();
    emit qsoRemoved(sourceRow);
}

void QsoListModel::updateQso(int row, const QsoRecord& qso)
{
    if (row < 0 || row >= m_qsos.count())
        return;

    m_qsos[row] = qso;
    int viewRow = mapFromSource(row);
    if (viewRow >= 0) {
        emit dataChanged(index(viewRow, 0), index(viewRow, m_columnHeaders.count() - 1));
    }
    emit qsoUpdated(row);
}

void QsoListModel::updateMultiplierCount(int row, int multCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;

    m_qsos[row].setMultiplierCount(multCount);

    int viewRow = mapFromSource(row);
    if (viewRow < 0) return;
    int multColIndex = m_columnHeaders.indexOf("M");
    if (multColIndex >= 0) {
        emit dataChanged(index(viewRow, multColIndex), index(viewRow, multColIndex));
    }
}

void QsoListModel::updateDxccCount(int row, int dxccCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;

    m_qsos[row].setDxccCount(dxccCount);

    int viewRow = mapFromSource(row);
    if (viewRow < 0) return;
    int dxccColIndex = m_columnHeaders.indexOf("C");
    if (dxccColIndex >= 0) {
        emit dataChanged(index(viewRow, dxccColIndex), index(viewRow, dxccColIndex));
    }
}

void QsoListModel::updateItuRegionCount(int row, int ituRegionCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;

    m_qsos[row].setItuRegionCount(ituRegionCount);

    int viewRow = mapFromSource(row);
    if (viewRow < 0) return;
    int ituColIndex = m_columnHeaders.indexOf("ITU");
    if (ituColIndex < 0)
        ituColIndex = m_columnHeaders.indexOf("Region");
    if (ituColIndex >= 0) {
        emit dataChanged(index(viewRow, ituColIndex), index(viewRow, ituColIndex));
    }
}

void QsoListModel::updateGridSquareMultiplier(int row, const QString& gridSquare)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    DebugLogger::instance().log("QsoListModel", QString("updateGridSquareMultiplier: row=%1, gridSquare='%2'").arg(row).arg(gridSquare));
    m_qsos[row].setGridSquareMultiplier(gridSquare);

    int viewRow = mapFromSource(row);
    if (viewRow >= 0) {
        int gridMultColIndex = m_columnHeaders.indexOf("GRID_MULT");
        DebugLogger::instance().log("QsoListModel", QString("  GRID_MULT column index: %1").arg(gridMultColIndex));
        if (gridMultColIndex >= 0) {
            emit dataChanged(index(viewRow, gridMultColIndex), index(viewRow, gridMultColIndex));
        }
    }
}

void QsoListModel::updateGridSquareMultiplierCount(int row, int gridSquareMultCount)
{
    if (row < 0 || row >= m_qsos.count())
        return;
    
    m_qsos[row].setGridSquareMultiplierCount(gridSquareMultCount);

    int viewRow = mapFromSource(row);
    if (viewRow < 0) return;
    int gridMultColIndex = m_columnHeaders.indexOf("GRID_MULT");
    if (gridMultColIndex >= 0) {
        emit dataChanged(index(viewRow, gridMultColIndex), index(viewRow, gridMultColIndex));
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
    m_visibleIndices.clear();
    endResetModel();
}

void QsoListModel::replaceAll(const QList<QsoRecord>& qsos)
{
    beginResetModel();
    m_qsos = qsos;
    rebuildVisibleIndices();
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

void QsoListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= m_columnHeaders.size() || m_qsos.isEmpty())
        return;

    emit layoutAboutToBeChanged();

    const QString header = m_columnHeaders.at(column).toUpper();
    const QString origHeader = m_columnHeaders.at(column);

    std::stable_sort(m_qsos.begin(), m_qsos.end(),
        [&](const QsoRecord& a, const QsoRecord& b) {
            bool lessThan = false;

            if (header == "#" || header == "SERIAL" || header == "NR") {
                lessThan = a.getSerial() < b.getSerial();
            } else if (header == "DATE" || header == "TIME") {
                lessThan = a.getDateTime() < b.getDateTime();
            } else if (header == "FREQUENCY" || header == "FREQ") {
                lessThan = a.getFrequency().toDouble() < b.getFrequency().toDouble();
            } else if (header == "CALL") {
                lessThan = a.getCall() < b.getCall();
            } else if (header == "MODE") {
                lessThan = a.getMode() < b.getMode();
            } else if (header == "RSTS") {
                lessThan = a.getRstSent() < b.getRstSent();
            } else if (header == "RSTR") {
                lessThan = a.getRstReceived() < b.getRstReceived();
            } else if (header == "DUPE") {
                lessThan = (a.isDupe() ? 1 : 0) < (b.isDupe() ? 1 : 0);
            } else if (header == "M") {
                lessThan = a.getMultiplierCount() < b.getMultiplierCount();
            } else if (header == "C") {
                lessThan = a.getDxccCount() < b.getDxccCount();
            } else if (header == "P" || header == "POINTS") {
                lessThan = a.getPoints() < b.getPoints();
            } else {
                // Exchange fields and anything else - string comparison
                lessThan = a.getExchangeField(origHeader) < b.getExchangeField(origHeader);
            }

            return order == Qt::AscendingOrder ? lessThan : !lessThan;
        });

    rebuildVisibleIndices();
    emit layoutChanged();
}

int QsoListModel::mapToSource(int viewRow) const
{
    if (m_visibleIndices.isEmpty())
        return viewRow;
    if (viewRow < 0 || viewRow >= m_visibleIndices.count())
        return -1;
    return m_visibleIndices.at(viewRow);
}

int QsoListModel::mapFromSource(int sourceRow) const
{
    if (m_visibleIndices.isEmpty())
        return sourceRow;
    return m_visibleIndices.indexOf(sourceRow);
}

void QsoListModel::rebuildVisibleIndices()
{
    m_visibleIndices.clear();
    if (m_filterText.isEmpty())
        return;
    for (int i = 0; i < m_qsos.count(); ++i) {
        if (matchesFilter(m_qsos.at(i)))
            m_visibleIndices.append(i);
    }
}

bool QsoListModel::matchesFilter(const QsoRecord& qso) const
{
    if (m_filterText.isEmpty())
        return true;
    const QString filter = m_filterText.toLower();
    const QStringList fields = {
        qso.getCall(),
        qso.getMode(),
        qso.getExchangeSent(),
        qso.getExchangeReceived(),
        qso.getComment(),
        qso.getFrequency(),
    };
    for (const QString& field : fields) {
        if (field.toLower().contains(filter))
            return true;
    }
    return false;
}

void QsoListModel::setFilter(const QString& text)
{
    beginResetModel();
    m_filterText = text;
    rebuildVisibleIndices();
    endResetModel();
}
