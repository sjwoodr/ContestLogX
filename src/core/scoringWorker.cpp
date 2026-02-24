#include "scoringWorker.h"
#include "debugLogger.h"

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

    // Single O(n) pass — rescoreAll handles dupe detection, points, mults, and running score.
    m_contestEngine->rescoreAll(m_qsos, m_myCallsign);

    DebugLogger::instance().log("ScoringWorker",
        QString("Finished scoring %1 QSOs on background thread").arg(m_qsos.count()));

    emit scoringComplete(m_qsos, true);
}
