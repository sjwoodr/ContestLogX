/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
