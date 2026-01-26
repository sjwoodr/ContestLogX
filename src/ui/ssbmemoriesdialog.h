#ifndef SSBMEMORIESDIALOG_H
#define SSBMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QList>
#include <QPushButton>
#include "ssbmemory.h"

class SsbMemoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SsbMemoriesDialog(QWidget *parent = nullptr);
    ~SsbMemoriesDialog();

    QList<SsbMemory> getMemories() const;
    void setMemories(const QList<SsbMemory>& memories);

private slots:
    void onSave();
    void onCancel();

private:
    void setupUi();

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];
};

#endif // SSBMEMORIESDIALOG_H
