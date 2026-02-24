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

#ifndef SHORTCUTSWIDGET_H
#define SHORTCUTSWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QMap>

class ShortcutsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutsWidget(QWidget* parent = nullptr);

    void saveShortcuts();

private slots:
    void onAddShortcut();
    void onEditShortcut();
    void onRemoveShortcut();
    void onResetDefaults();

private:
    void loadShortcuts();
    void setupUI();
    void populateTable();

    QTableWidget* m_table;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_removeButton;
    QPushButton* m_resetButton;

    QMap<QString, QString> m_shortcuts;
    QMap<QString, QString> m_originalShortcuts;

    struct DefaultShortcut {
        QString action;
        QString description;
        QString defaultKey;
    };

    static const QList<DefaultShortcut> DEFAULT_SHORTCUTS;
};

#endif // SHORTCUTSWIDGET_H
