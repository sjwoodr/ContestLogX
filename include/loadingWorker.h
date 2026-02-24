#ifndef LOADINGWORKER_H
#define LOADINGWORKER_H

#include <QObject>
#include <QString>
#include <QList>
#include "qsoRecord.h"

class LoadingWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit LoadingWorker(const QString& fileName, QObject *parent = nullptr);
    
public slots:
    void doLoad();
    
signals:
    void progressUpdated(int current, int total);
    void loadingComplete(QList<QsoRecord> qsos, bool success, QString errorMessage);
    
private:
    QString m_fileName;
};

#endif // LOADINGWORKER_H
