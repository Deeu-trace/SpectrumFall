#pragma once

#include <QVector>
#include "BeatDetector.h"

/// 音符类型
enum NoteType { TAP = 0, HOLD = 1 };

/// 游戏音符数据结构
struct GameNote
{
    qint64 timestampMs;    ///< 音符时间戳（毫秒）
    int lane;              ///< 轨道编号（0=D, 1=F, 2=J, 3=K）
    bool judged;           ///< 是否已判定
    int judgment;          ///< 判定结果（0=未判定, 1=Perfect, 2=Good, 3=Miss）
    int noteType;          ///< 音符类型（0=TAP, 1=HOLD）
    qint64 holdDurationMs; ///< Hold 持续时间（毫秒），TAP 为 0
    bool holdActive;       ///< Hold 是否正在按住中
    bool holdFinished;     ///< Hold 尾端是否已完成判定

    GameNote()
        : timestampMs(0), lane(0), judged(false), judgment(0)
        , noteType(TAP), holdDurationMs(0), holdActive(false), holdFinished(false) {}

    GameNote(qint64 ts, int ln)
        : timestampMs(ts), lane(ln), judged(false), judgment(0)
        , noteType(TAP), holdDurationMs(0), holdActive(false), holdFinished(false) {}

    GameNote(qint64 ts, int ln, int type, qint64 dur)
        : timestampMs(ts), lane(ln), judged(false), judgment(0)
        , noteType(type), holdDurationMs(dur), holdActive(false), holdFinished(false) {}

    /// Hold 尾端时间戳
    qint64 holdEndMs() const { return timestampMs + holdDurationMs; }
};

/// 音符生成器：将 BeatPoint 列表转换为 GameNote，过滤过近的节拍
class NoteGenerator
{
public:
    NoteGenerator() = default;

    /// 从节拍点生成游戏音符
    /// @param laneCount 轨道数（4 或 6），用于兜底
    QVector<GameNote> generate(const QVector<BeatPoint>& beatPoints, qint64 minGapMs = 200, int laneCount = 6);
};
