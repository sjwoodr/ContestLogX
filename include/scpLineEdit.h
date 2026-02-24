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

#ifndef SCPLINEEDIT_H
#define SCPLINEEDIT_H

#include <QLineEdit>

class ScpWidget;

class ScpLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ScpLineEdit(QWidget *parent = nullptr);
    
    // Set the SCP widget to update when search changes
    void setScpWidget(ScpWidget *widget);
    
    // Enable/disable SCP search
    void setScpEnabled(bool enabled);
    bool isScpEnabled() const { return m_scpEnabled; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void performScpSearch();
    void onScpCallsignSelected(const QString& callsign);

private:
    void updateScpWidget();
    
    ScpWidget *m_scpWidget;
    bool m_scpEnabled;
};

#endif // SCPLINEEDIT_H

