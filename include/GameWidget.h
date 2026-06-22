#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QVector>
#include <QPixmap>
#include "NoteGenerator.h"

class AudioEngine;
class ScoreManager;
class QPushButton;
class QSlider;
class QLabel;

/// 命中特效粒子
struct HitParticle {
    qreal x, y;
    qreal vx, vy;
    QColor color;
    int age;
    int maxAge;
    qreal size;
};

/// 命中反馈环
struct HitRing {
    qreal x, y;
    QColor color;
    int age;
    int maxAge;
    qreal startRadius;
    qreal endRadius;
};

/// 分数弹出动画
struct ScorePopup {
    QString text;
    QColor color;
    qreal x, y;
    int age;
    int maxAge;
};

/// 游戏页面：下落式音符，判定线，键盘输入，判定文字动画
/// 支持 4 键 (D/F/J/K) 和 6 键 (S/D/F/J/K/L) 模式
class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(AudioEngine* audioEngine, ScoreManager* scoreManager, QWidget* parent = nullptr);

    /// 开始游戏（传入音符列表）
    /// @param laneCount 轨道数（4 或 6）
    void startGame(const QVector<GameNote>& notes, int laneCount = 6);

    /// 暂停游戏
    void pauseGame();

    /// 恢复游戏
    void resumeGame();

signals:
    void gameFinished();
    void backRequested();
    void restartRequested();  ///< 请求 MainWindow 重新开始当前歌曲

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRenderTick();

private:
    void setupPauseOverlay();          ///< 创建暂停菜单遮罩

    /// 根据当前密度设置过滤音符列表
    void applyDensityFilter();

    /// 将同轨道间距相近的 Tap 音符合并为 Hold 音符
    void mergeHolds();

    /// 处理 Hold 尾部释放判定
    void judgeHoldRelease(int lane);

    AudioEngine* m_audioEngine;        ///< 音频引擎
    ScoreManager* m_scoreManager;      ///< 分数管理器
    QTimer* m_renderTimer;             ///< 渲染定时器
    QVector<GameNote> m_notes;         ///< 当前游戏音符列表（可能已过滤）
    QVector<GameNote> m_allNotes;      ///< 原始完整音符列表（未过滤）
    QMap<int, bool> m_keyPressed;      ///< 按键状态
    QMap<int, int> m_activeHolds;      ///< 活跃 Hold：lane → m_notes 索引
    bool m_paused;                     ///< 是否暂停
    bool m_gameActive;                 ///< 游戏是否活跃
    int m_minGapMs;                    ///< 同轨道音符最小间隔（毫秒）
    int m_globalMinGapMs;              ///< 跨轨道全局最小间隔（毫秒）
    bool m_densityChanged;             ///< 密度是否被修改过

    // 精确计时
    QElapsedTimer m_gameClock;         ///< 游戏高精度时钟
    qint64 m_startPosMs;               ///< 游戏开始时的音频位置
    qint64 m_pauseElapsedMs;           ///< 暂停时已经过的时间
    qint64 m_totalPausedMs;            ///< 累计暂停时间

    // 判定文字动画
    QString m_judgeText;               ///< 当前判定文字
    int m_judgeTextLane;               ///< 判定文字所在轨道
    int m_judgeTextTimer;              ///< 判定文字显示计时器（帧数）
    QColor m_judgeTextColor;           ///< 判定文字颜色

    // 判定线和轨道参数
    qreal m_judgeLineY;                ///< 判定线 Y 坐标
    qreal m_noteSpeed;                 ///< 音符下落速度（像素/毫秒）
    qreal m_trackWidth;                ///< 轨道宽度
    int m_laneCount;                   ///< 轨道数（4 或 6）

    // ── 炫酷效果 ──
    QVector<QPointF> m_stars;          ///< 预生成的星光位置
    QVector<float> m_starPhases;       ///< 星光闪烁相位
    QVector<HitParticle> m_particles;  ///< 命中粒子
    QVector<HitRing> m_rings;          ///< 命中反馈环
    QVector<ScorePopup> m_popups;      ///< 分数弹出
    qreal m_judgeLinePulse;            ///< 判定线脉冲强度（0-1，命中时跳到1然后衰减）
    qreal m_comboScale;                ///< Combo 文字缩放动画
    int m_lastCombo;                   ///< 上一帧的 combo 值（用于检测变化）
    qint64 m_lastFrameTime;            ///< 上一帧时间戳
    int m_holdSparkleCounter;          ///< Hold 持续粒子生成计数器

    // 暂停菜单
    QWidget* m_pauseOverlay;           ///< 暂停遮罩
    QPushButton* m_continueBtn;        ///< 继续游戏按钮
    QPushButton* m_backToMenuBtn;      ///< 返回主菜单按钮
    QSlider* m_densitySlider;          ///< 同轨道密度滑块
    QLabel* m_densityLabel;            ///< 同轨道密度值显示
    QSlider* m_globalGapSlider;        ///< 跨轨道全局间隔滑块
    QLabel* m_globalGapLabel;          ///< 跨轨道全局间隔值显示

    /// 获取精确的当前游戏时间（毫秒），基于高精度时钟
    qint64 getGameTime() const;

    /// 检查超时未击打的音符（MISS 判定）
    void checkMissedNotes();

    /// 处理按键判定
    void judgeLane(int lane);

    /// 获取轨道颜色
    QColor laneColor(int lane) const;

    /// 获取轨道 X 坐标
    qreal laneX(int lane) const;

    /// 生成命中特效（粒子+环+脉冲）
    void spawnHitEffect(int lane, int judgment);

    /// Hold 头部命中特效（增强爆发）
    void spawnHoldHeadEffect(int lane, int judgment);

    /// Hold 完成时的特效
    void spawnHoldCompleteEffect(int lane, int judgment);

    /// 更新粒子/环/动画状态
    void updateEffects(qint64 deltaTimeMs);

    /// 生成星光
    void generateStars();

    /// 重建缓存背景
    void rebuildBackground();

    QPixmap m_bgCache;                ///< 缓存的背景（星空+渐变）
    QSize m_bgCacheSize;              ///< 缓存背景的尺寸（用于判断是否需要重建）
};
