#ifndef CWMEMORIESDIALOG_H
#define CWMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QList>
#include <QPushButton>
#include "cwmemory.h"

class CwMemoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CwMemoriesDialog(QWidget *parent = nullptr);
    ~CwMemoriesDialog();
    
    QList<CwMemory> getMemories() const;
    void setMemories(const QList<CwMemory>& memories);

private slots:
    void onSave();
    void onCancel();

private:
    void setupUi();
    
    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];
};

#endif // CWMEMORIESDIALOG_H
