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

#ifndef CONTESTSELECTDIALOG_H
#define CONTESTSELECTDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class ContestSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContestSelectDialog(QWidget *parent = nullptr);
    QString selectedContestFile() const;
    bool isOpeningExisting() const { return m_openingExisting; }

private slots:
    void onOkClicked();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onOpenExistingClicked();

private:
    void loadContestList();
    
    QListWidget *m_contestList;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QPushButton *m_openExistingButton;
    QString m_selectedFile;
    bool m_openingExisting;
};

#endif // CONTESTSELECTDIALOG_H
