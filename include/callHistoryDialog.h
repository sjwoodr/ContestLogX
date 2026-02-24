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

#ifndef CALLHISTORYDIALOG_H
#define CALLHISTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>
#include <QPushButton>

/**
 * @brief Dialog for managing call history
 */
class CallHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CallHistoryDialog(QWidget *parent = nullptr);
    
protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onTableItemDoubleClicked(int row, int column);
    void onTableCustomContextMenu(const QPoint &pos);
    void onEditRecord();
    void onDeleteRecord();
    void onEnableToggled(bool checked);
    void onAutoSaveToggled(bool checked);
    void onClearHistory();
    void refreshTable();
    
private:
    void setupUI();
    void editRecord(int row);
    void deleteRecord(int row);
    
    QTableWidget* m_historyTable;
    QCheckBox* m_enableCheckBox;
    QCheckBox* m_autoSaveCheckBox;
    QPushButton* m_clearButton;
    int m_contextMenuRow = -1;
};

#endif // CALLHISTORYDIALOG_H
