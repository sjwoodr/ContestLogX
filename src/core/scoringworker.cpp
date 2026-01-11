#include "scoringworker.h"
#include "debuglogger.h"

ScoringWorker::ScoringWorker(QList<QsoRecord> qsos, ContestEngine* contestEngine,
                             const QString& myCallsign, QObject *parent)
    : QObject(parent)
    , m_qsos(qsos)
    , m_contestEngine(contestEngine)
    , m_myCallsign(myCallsign)
{
}

void ScoringWorker::doScore()
{
    if (!m_contestEngine) {
        DebugLogger::instance().log("ScoringWorker", "Error: ContestEngine is null");
        emit scoringComplete(m_qsos, false);
        return;
    }
    
    DebugLogger::instance().log("ScoringWorker", 
        QString("Starting to score %1 QSOs on background thread").arg(m_qsos.count()));
    
    // Reset contest engine
    m_contestEngine->resetScore();
    
    // Re-score each QSO
    for (int i = 0; i < m_qsos.count(); ++i) {
        QsoRecord qso = m_qsos[i];
        
        // Check if out-of-band
        double freqKhz = qso.getFrequency().toDouble();
        if (!m_contestEngine->isValidBand(freqKhz)) {
            qso.setOutOfBand(true);
            qso.setComment("Out of band for contest");
            qso.setPoints(0);
            qso.setDupe(false);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            m_qsos[i] = qso;
            
            if ((i + 1) % 100 == 0) {
                emit progressUpdated(i + 1, m_qsos.count());
            }
            continue;
        }
        
        // Reset dupe and out-of-band flags
        qso.setOutOfBand(false);
        qso.setDupe(false);
        qso.setComment("");
        
        // Check for duplicates (against previously scored QSOs)
        QList<QsoRecord> previousQsos = m_qsos.mid(0, i);
        bool isDupe = m_contestEngine->isDupe(qso, previousQsos);
        
        if (isDupe) {
            qso.setDupe(true);
            qso.setPoints(0);
            qso.setMultiplierCount(0);
            qso.setDxccCount(0);
            QString dupeReason = m_contestEngine->getDupeReason(qso, previousQsos);
            qso.setComment(QString("Duplicate contact for %1").arg(dupeReason));
            m_qsos[i] = qso;
            
            if ((i + 1) % 100 == 0) {
                emit progressUpdated(i + 1, m_qsos.count());
            }
            continue;
        }
        
        // Calculate points
        int points = m_contestEngine->calculatePoints(qso, m_myCallsign);
        qso.setPoints(points);
        
        // Get per-QSO multiplier credit
        ContestEngine::QsoMultiplierCredit credit = m_contestEngine->getQsoMultiplierCredit(qso, previousQsos);
        qso.setMultiplierCount(credit.namedMultCount);
        qso.setDxccCount(credit.dxccMultCount);
        qso.setItuRegionCount(credit.ituRegionMultCount);
        
        m_qsos[i] = qso;
        
        // Emit progress every 100 QSOs
        if ((i + 1) % 100 == 0) {
            emit progressUpdated(i + 1, m_qsos.count());
        }
    }
    
    // Final update of running score
    m_contestEngine->updateRunningScore(m_qsos, m_myCallsign, false);
    
    DebugLogger::instance().log("ScoringWorker", 
        QString("Finished scoring %1 QSOs on background thread").arg(m_qsos.count()));
    
    emit scoringComplete(m_qsos, true);
}
