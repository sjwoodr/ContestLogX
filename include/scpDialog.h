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

#ifndef SCPDIALOG_H
#define SCPDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

class ScpWidget;

class ScpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScpDialog(ScpWidget *scpWidget, QWidget *parent = nullptr);
    
    bool isScpEnabled() const;
    void setScpEnabled(bool enabled);

private slots:
    void onDownloadClicked();
    void onScpToggled(bool checked);

private:
    void setupUi();
    void updateDatabaseInfo();
    
    ScpWidget *m_scpWidget;
    QPushButton *m_downloadButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QLabel *m_databaseInfoLabel;
    QCheckBox *m_enableCheckBox;
};

#endif // SCPDIALOG_H
