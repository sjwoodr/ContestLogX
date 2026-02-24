#ifndef CABRILLOEXPORT_H
#define CABRILLOEXPORT_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include "qsoRecord.h"

class CabrilloExport
{
public:
    CabrilloExport();
    
    bool exportToFile(const QString& filename,
                      const QList<QsoRecord>& qsos,
                      const QJsonObject& contestDef,
                      const QJsonObject& headerData,
                      const QString& myCall = QString(),
                      const QString& selectedMode = QString());
    
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
    QString m_myCall;
    
    QString generateHeader(const QJsonObject& contestDef, const QJsonObject& headerData, const QString& selectedMode = QString());
    QString generateQsoLine(const QsoRecord& qso, const QString& qsoTemplate);
    QString formatFrequency(double freqKhz);
    QString formatFrequency(const QString& freq);
    bool isHeaderRequired(const QString& headerName, const QJsonArray& requiredHeaders);
};

#endif // CABRILLOEXPORT_H
