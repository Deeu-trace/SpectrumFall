#include "NoteGenerator.h"

QVector<GameNote> NoteGenerator::generate(const QVector<BeatPoint>& beatPoints, qint64 minGapMs)
{
    QVector<GameNote> notes;

    qint64 lastTimestamp = -minGapMs; // 确保第一个节拍不被过滤

    for (const BeatPoint& bp : beatPoints) {
        // 过滤间隔太近的节拍
        if (bp.timestampMs - lastTimestamp < minGapMs) {
            continue;
        }

        GameNote note(bp.timestampMs, bp.lane);
        notes.append(note);
        lastTimestamp = bp.timestampMs;
    }

    return notes;
}
