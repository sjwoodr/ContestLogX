/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
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
