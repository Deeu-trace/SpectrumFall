#pragma once

#include <QObject>

/// 分数管理器：管理判定、得分、连击等游戏数据
class ScoreManager : public QObject
{
    Q_OBJECT

public:
    explicit ScoreManager(QObject* parent = nullptr);

    /// 重置所有数据
    void reset();

    /// 判定击打：返回 1=Perfect, 2=Good, 3=Miss
    int judgeHit(qint64 hitTimeMs, qint64 noteTimeMs);

    /// 记录一次 Miss
    void addMiss();

    /// 获取总分
    int score() const;

    /// 获取当前连击
    int combo() const;

    /// 获取最大连击
    int maxCombo() const;

    /// 获取 Perfect 数量
    int perfectCount() const;

    /// 获取 Good 数量
    int goodCount() const;

    /// 获取 Miss 数量
    int missCount() const;

    /// 计算评级
    QString grade(int totalNotes) const;

signals:
    void scoreChanged(int score);
    void comboChanged(int combo);

private:
    int m_score;          ///< 总分
    int m_combo;          ///< 当前连击
    int m_maxCombo;       ///< 最大连击
    int m_perfectCount;   ///< Perfect 数量
    int m_goodCount;      ///< Good 数量
    int m_missCount;      ///< Miss 数量

    static constexpr qint64 PERFECT_WINDOW = 80;    ///< Perfect 判定窗口 ±80ms（补偿 QMediaPlayer 延迟）
    static constexpr qint64 GOOD_WINDOW = 150;       ///< Good 判定窗口 ±150ms
    static constexpr int PERFECT_SCORE = 300;       ///< Perfect 得分
    static constexpr int GOOD_SCORE = 100;          ///< Good 得分
};
