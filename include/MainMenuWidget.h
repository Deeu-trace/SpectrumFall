#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>

class QPushButton;

/// 主菜单页面：粒子系统 + 频谱条 + 下落光带 + 脉冲辉光
class MainMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenuWidget(QWidget* parent = nullptr);

signals:
    void songSelectRequested();
    void leaderboardRequested();
    void exitRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPushButton* m_selectSongBtn;   ///< 选择歌曲按钮
    QPushButton* m_leaderboardBtn;  ///< 排行榜按钮
    QPushButton* m_exitBtn;         ///< 退出按钮

    float m_bgPhase;                ///< 全局动画相位

    // ── 粒子系统 ──
    struct Particle {
        float x, y;       ///< 归一化坐标 [0,1]
        float vx, vy;     ///< 每帧速度
        float radius;     ///< 粒子半径(px)
        QColor color;     ///< 基色
        float phaseOff;   ///< alpha 脉冲相位偏移
    };
    QVector<Particle> m_particles;

    // ── 频谱条 ──
    static constexpr int SPECTRUM_BARS = 32;
    float m_barHeight[SPECTRUM_BARS];
    float m_barTarget[SPECTRUM_BARS];
    int   m_barTick;

    void initParticles(int count);
    void tickSpectrum();
};
