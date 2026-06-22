#include "NoteGenerator.h"

QVector<GameNote> NoteGenerator::generate(const QVector<BeatPoint>& beatPoints, qint64 minGapMs, int laneCount)
{
    QVector<GameNote> notes;

    QVector<qint64> lastTimestampPerLane(laneCount, -minGapMs);

    for (const BeatPoint& bp : beatPoints) {
        int lane = bp.lane;
        if (lane < 0 || lane >= laneCount) lane = 1;

        if (bp.timestampMs - lastTimestampPerLane[lane] < minGapMs) {
            continue;
        }

        GameNote note(bp.timestampMs, lane);
        notes.append(note);
        lastTimestampPerLane[lane] = bp.timestampMs;
    }

    return notes;
}
