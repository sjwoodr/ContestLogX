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

#ifndef SCPWIDGET_H
#define SCPWIDGET_H

#include <QDockWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

class ScpWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit ScpWidget(QWidget *parent = nullptr);
    
    // Update the SCP results table
    void updateResults(const QStringList& callsigns);
    
    // Clear the table
    void clearResults();
    
    // Get the selected callsign
    QString getSelectedCallsign() const;
    
    // Set the search prefix display
    void setSearchPrefix(const QString& prefix);
    
    // Update the title based on enabled/disabled state
    void updateTitle();

signals:
    // Emitted when a callsign is selected (double-clicked)
    void callsignSelected(const QString& callsign);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCellDoubleClicked(int row, int column);
    void layoutTable();

private:
    void setupUi();
    int getColumnCount() const;
    
    QTableWidget *m_callsignTable;
};

#endif // SCPWIDGET_H
