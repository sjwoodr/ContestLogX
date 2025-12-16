/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#ifndef SHORTCUTSDIALOG_H
#define SHORTCUTSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QMap>

class ShortcutsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);
    
    QMap<QString, QString> getShortcuts() const;

private slots:
    void onAddShortcut();
    void onEditShortcut();
    void onRemoveShortcut();
    void onResetDefaults();
    void onAccepted();

private:
    void loadShortcuts();
    void setupUI();
    void populateTable();
    
    QTableWidget* m_table;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_removeButton;
    QPushButton* m_resetButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    QMap<QString, QString> m_shortcuts;
    
    struct DefaultShortcut {
        QString action;
        QString description;
        QString defaultKey;
    };
    
    static const QList<DefaultShortcut> DEFAULT_SHORTCUTS;
};

#endif // SHORTCUTSDIALOG_H
