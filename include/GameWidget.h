#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QVector>
#include "NoteGenerator.h"

class AudioEngine;
class ScoreManager;
class QPushButton;

/// 游戏页面：4 轨道下落式音符，判定线，键盘输入 D/F/J/K，判定文字动画
class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(AudioEngine* audioEngine, ScoreManager* scoreManager, QWidget* parent = nullptr);

    /// 开始游戏（传入音符列表）
    void startGame(const QVector<GameNote>& notes);

    /// 暂停游戏
    void pauseGame();

    /// 恢复游戏
    void resumeGame();

signals:
    void gameFinished();
    void backRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRenderTick();

private:
    void setupPauseOverlay();          ///< 创建暂停菜单遮罩

    AudioEngine* m_audioEngine;        ///< 音频引擎
    ScoreManager* m_scoreManager;      ///< 分数管理器
    QTimer* m_renderTimer;             ///< 渲染定时器
    QVector<GameNote> m_notes;         ///< 游戏音符列表
    QMap<int, bool> m_keyPressed;      ///< 按键状态
    bool m_paused;                     ///< 是否暂停
    bool m_gameActive;                 ///< 游戏是否活跃

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

    // 暂停菜单
    QWidget* m_pauseOverlay;           ///< 暂停遮罩
    QPushButton* m_continueBtn;        ///< 继续游戏按钮
    QPushButton* m_backToMenuBtn;      ///< 返回主菜单按钮

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
};
