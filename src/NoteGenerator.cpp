#include "NoteGenerator.h"

QVector<GameNote> NoteGenerator::generate(const QVector<BeatPoint>& beatPoints, qint64 minGapMs)
{
    QVector<GameNote> notes;

    // 每个轨道独立追踪最近时间戳，允许同一时刻不同轨道的双押共存
    qint64 lastTimestampPerLane[4] = {-minGapMs, -minGapMs, -minGapMs, -minGapMs};

    for (const BeatPoint& bp : beatPoints) {
        int lane = bp.lane;
        if (lane < 0 || lane > 3) lane = 0; // 安全兜底

        // 同一轨道内间隔太近才过滤
        if (bp.timestampMs - lastTimestampPerLane[lane] < minGapMs) {
            continue;
        }

        GameNote note(bp.timestampMs, lane);
        notes.append(note);
        lastTimestampPerLane[lane] = bp.timestampMs;
    }

    return notes;
}
