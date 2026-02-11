#ifndef SSBMEMORIESDIALOG_H
#define SSBMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
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

    // Contest-specific memory support
    void setContestMemories(const QList<SsbMemory>& memories);
    void setContestMode(bool contest);
    bool isContestMode() const;

private slots:
    void onSave();
    void onCancel();
    void onModeToggled();

private:
    void setupUi();
    void loadMemoriesToUi(const QList<SsbMemory>& memories);
    QList<SsbMemory> getMemoriesFromUi() const;

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];

    // Station/Contest radio buttons
    QRadioButton *m_stationRadio;
    QRadioButton *m_contestRadio;

    // Internal storage for both sets
    QList<SsbMemory> m_stationMemories;
    QList<SsbMemory> m_contestMemories;
    bool m_currentIsContest = false;
};

#endif // SSBMEMORIESDIALOG_H
