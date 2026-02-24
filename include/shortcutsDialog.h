/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
