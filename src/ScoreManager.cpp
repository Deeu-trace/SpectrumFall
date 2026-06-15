#include "ScoreManager.h"
#include <QtMath>

ScoreManager::ScoreManager(QObject* parent)
    : QObject(parent)
    , m_score(0)
    , m_combo(0)
    , m_maxCombo(0)
    , m_perfectCount(0)
    , m_goodCount(0)
    , m_missCount(0)
{
}

void ScoreManager::reset()
{
    m_score = 0;
    m_combo = 0;
    m_maxCombo = 0;
    m_perfectCount = 0;
    m_goodCount = 0;
    m_missCount = 0;
}

int ScoreManager::judgeHit(qint64 hitTimeMs, qint64 noteTimeMs)
{
    qint64 delta = qAbs(hitTimeMs - noteTimeMs);

    if (delta <= PERFECT_WINDOW) {
        m_score += PERFECT_SCORE;
        m_combo++;
        m_perfectCount++;
        if (m_combo > m_maxCombo) m_maxCombo = m_combo;
        emit scoreChanged(m_score);
        emit comboChanged(m_combo);
        return 1; // Perfect
    } else if (delta <= GOOD_WINDOW) {
        m_score += GOOD_SCORE;
        m_combo++;
        m_goodCount++;
        if (m_combo > m_maxCombo) m_maxCombo = m_combo;
        emit scoreChanged(m_score);
        emit comboChanged(m_combo);
        return 2; // Good
    } else {
        m_combo = 0;
        m_missCount++;
        emit comboChanged(m_combo);
        return 3; // Miss
    }
}

void ScoreManager::addMiss()
{
    m_missCount++;
    m_combo = 0;
    emit comboChanged(m_combo);
}

int ScoreManager::score() const { return m_score; }
int ScoreManager::combo() const { return m_combo; }
int ScoreManager::maxCombo() const { return m_maxCombo; }
int ScoreManager::perfectCount() const { return m_perfectCount; }
int ScoreManager::goodCount() const { return m_goodCount; }
int ScoreManager::missCount() const { return m_missCount; }

QString ScoreManager::grade(int totalNotes) const
{
    if (totalNotes <= 0) return QStringLiteral("D");

    int maxScore = totalNotes * PERFECT_SCORE;
    float ratio = static_cast<float>(m_score) / maxScore;

    if (ratio >= 0.95f) return QStringLiteral("S");
    if (ratio >= 0.80f) return QStringLiteral("A");
    if (ratio >= 0.60f) return QStringLiteral("B");
    if (ratio >= 0.40f) return QStringLiteral("C");
    return QStringLiteral("D");
}
