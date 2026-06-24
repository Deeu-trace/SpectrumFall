#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

/// 排行榜条目：一次游戏成绩
struct LeaderboardEntry
{
    QString songFileName;   ///< 歌曲文件名（显示用）
    qint64  songFileSize;   ///< 文件大小（字节），作为歌曲指纹
    qint64  songDurationMs; ///< 音频时长（毫秒）
    float   bpm;            ///< 检测到的 BPM
    int     laneCount;      ///< 4 或 6（不同键数各自一榜）
    QString playerName;     ///< 玩家名
    int     score;          ///< 总分
    int     perfect;        ///< Perfect 数
    int     good;           ///< Good 数
    int     miss;           ///< Miss 数
    int     maxCombo;       ///< 最大连击
    int     totalNotes;     ///< 谱面音符总数
    QString grade;          ///< 评级 S/A/B/C/D
    QDateTime playedAt;     ///< 游玩时间（唯一键）
};

/// 排行榜管理器：本地 JSON 文件持久化（镜像 CacheManager 范式）
/// 每首歌 + 每种键数各自一榜，按分数降序，每榜保留前 50 名。
class LeaderboardManager : public QObject
{
    Q_OBJECT

public:
    explicit LeaderboardManager(QObject* parent = nullptr);

    /// 获取所有条目
    QVector<LeaderboardEntry> allEntries() const;

    /// 获取指定歌曲+键数的全部条目（按分数降序，同分新者靠前）
    QVector<LeaderboardEntry> entriesForSong(qint64 songFileSize, int laneCount) const;

    /// 获取指定歌曲+键数的前 N 名（默认 50）
    QVector<LeaderboardEntry> topEntries(qint64 songFileSize, int laneCount, int limit = 50) const;

    /// 添加一条成绩。返回本次排名（1-based）；返回 0 表示未进入前 50 名（已被截断）。
    int addEntry(const LeaderboardEntry& entry);

    /// 删除指定条目（按 游玩时间+玩家名+分数 匹配）
    void removeEntry(const QDateTime& playedAt, const QString& playerName, int score);

    /// 清空指定歌曲+键数的整张榜
    void clearSong(qint64 songFileSize, int laneCount);

    /// 本机默认玩家名（用于结算页预填）
    QString myName() const;
    void setMyName(const QString& name);

private:
    QString m_path;                     ///< 排行榜文件路径
    QString m_myName;                   ///< 本机玩家名
    QVector<LeaderboardEntry> m_entries;///< 内存中的全部条目

    /// 从磁盘加载
    void loadFromDisk();

    /// 保存到磁盘
    void saveToDisk();

    /// 按 (歌曲,键数) 分组、分数降序、每榜保留前 50
    void trimAndSort();
};
