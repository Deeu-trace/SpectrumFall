#include "MainMenuWidget.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QFont>
#include <QRandomGenerator>
#include <cmath>

// ── 构造 ──────────────────────────────────────────────────────
MainMenuWidget::MainMenuWidget(QWidget* parent)
    : QWidget(parent)
    , m_bgPhase(0.0f)
    , m_barTick(0)
{
    initParticles(55);
    for (int i = 0; i < SPECTRUM_BARS; ++i) {
        m_barHeight[i] = 0.0f;
        m_barTarget[i]  = 0.0f;
    }

    // ── 布局 ──
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(0);

    layout->addStretch(2);

    // 标题
    QLabel* titleLabel = new QLabel(QStringLiteral("SpectrumFall"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(72);
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    layout->addSpacing(60);

    auto makeBtn = [&](const QString& text) -> QPushButton* {
        QPushButton* btn = new QPushButton(text, this);
        btn->setMinimumSize(260, 54);
        btn->setObjectName("menuButton");
        layout->addWidget(btn, 0, Qt::AlignCenter);
        layout->addSpacing(14);
        return btn;
    };

    m_selectSongBtn  = makeBtn(QStringLiteral("开 始 游 戏"));
    m_leaderboardBtn = makeBtn(QStringLiteral("排  行  榜"));
    m_exitBtn        = makeBtn(QStringLiteral("退    出"));

    // 去掉最后一个多余的 spacing
    layout->addStretch(3);

    // 信号
    connect(m_selectSongBtn,  &QPushButton::clicked, this, &MainMenuWidget::songSelectRequested);
    connect(m_leaderboardBtn, &QPushButton::clicked, this, &MainMenuWidget::leaderboardRequested);
    connect(m_exitBtn,        &QPushButton::clicked, this, &MainMenuWidget::exitRequested);

    // 动画定时器 ~60fps
    QTimer* anim = new QTimer(this);
    connect(anim, &QTimer::timeout, this, [this]() {
        m_bgPhase += 0.012f;
        if (m_bgPhase > 6.2832f) m_bgPhase -= 6.2832f;

        // 粒子运动
        auto* rng = QRandomGenerator::global();
        for (auto& p : m_particles) {
            p.x += p.vx;
            p.y += p.vy;
            if (p.y > 1.05f) {
                p.y = -0.05f;
                p.x = static_cast<float>(rng->bounded(1.0));
            }
            if (p.x < -0.05f) p.x = 1.05f;
            if (p.x >  1.05f) p.x = -0.05f;
        }

        tickSpectrum();
        update();
    });
    anim->start(16);
}

// ── 粒子初始化 ─────────────────────────────────────────────────
void MainMenuWidget::initParticles(int count)
{
    m_particles.clear();
    m_particles.reserve(count);

    const QColor palette[] = {
        QColor(  0, 255, 136),   // 绿
        QColor(  0, 200, 255),   // 青
        QColor(189, 147, 249),   // 紫
        QColor(255, 121, 198),   // 粉
        QColor(139, 233, 253),   // 浅蓝
        QColor(241, 250, 140),   // 黄
    };
    constexpr int palN = sizeof(palette) / sizeof(palette[0]);
    auto* rng = QRandomGenerator::global();

    for (int i = 0; i < count; ++i) {
        Particle p;
        p.x       = static_cast<float>(rng->bounded(1.0));
        p.y       = static_cast<float>(rng->bounded(1.0));
        p.vx      = static_cast<float>((rng->bounded(1.0) - 0.5) * 0.0006);
        p.vy      = static_cast<float>(rng->bounded(1.0) * 0.0008 + 0.0004);   // 向下飘 [0.0004, 0.0012)
        p.radius  = static_cast<float>(rng->bounded(15, 50)) / 10.0f;
        p.color   = palette[rng->bounded(palN)];
        p.phaseOff = static_cast<float>(rng->bounded(6.2832));
        m_particles.append(p);
    }
}

// ── 频谱条动画 ─────────────────────────────────────────────────
void MainMenuWidget::tickSpectrum()
{
    ++m_barTick;
    auto* rng = QRandomGenerator::global();
    // 每 6 tick 换一批新目标
    if (m_barTick % 6 == 0) {
        for (int i = 0; i < SPECTRUM_BARS; ++i) {
            float base = 0.35f * (1.0f - static_cast<float>(i) / SPECTRUM_BARS);
            m_barTarget[i] = base + static_cast<float>(rng->bounded(0.45));
        }
    }
    for (int i = 0; i < SPECTRUM_BARS; ++i) {
        m_barHeight[i] += (m_barTarget[i] - m_barHeight[i]) * 0.13f;
    }
}

// ── 绘制 ──────────────────────────────────────────────────────
void MainMenuWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // ── 1. 深色渐变背景 ──
    QLinearGradient bgGrad(0, 0, w, h);
    bgGrad.setColorAt(0.0,  QColor( 8,  6, 22));
    bgGrad.setColorAt(0.5,  QColor(18,  8, 38));
    bgGrad.setColorAt(1.0,  QColor( 6, 14, 30));
    painter.fillRect(rect(), bgGrad);

    // ── 2. 中心脉冲辉光 ──
    float pulse = 0.5f + 0.35f * std::sin(m_bgPhase * 1.3f);
    int glowR   = qMin(w, h) / 2;
    QRadialGradient cGlow(QPointF(w * 0.5, h * 0.33), glowR);
    cGlow.setColorAt(0.0, QColor(  0, 200, 120, static_cast<int>(55 * pulse)));
    cGlow.setColorAt(0.4, QColor( 80,   0, 200, static_cast<int>(28 * pulse)));
    cGlow.setColorAt(1.0, QColor(  0,   0,   0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(cGlow);
    painter.drawRect(rect());

    // ── 3. 下落光带（节奏音符感） ──
    static const QColor streakPal[] = {
        QColor(255,  85,  85),  // 红
        QColor(255, 184,  61),  // 橙
        QColor(241, 250, 140),  // 黄
        QColor( 80, 250, 123),  // 绿
        QColor(139, 233, 253),  // 蓝
        QColor(189, 147, 249),  // 紫
    };
    constexpr int STREAK_N = 10;
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < STREAK_N; ++i) {
        float phase = m_bgPhase * 0.45f + i * 0.63f;
        float yNorm = std::fmod(phase * 0.28f, 1.0f);
        float xNorm = 0.04f + (static_cast<float>(i) / STREAK_N) * 0.92f;
        float fade  = 1.0f - std::abs(yNorm - 0.5f) * 2.0f;
        int alpha   = static_cast<int>(fade * 70);
        if (alpha <= 0) continue;

        QColor c = streakPal[i % 6];
        float px  = xNorm * w;
        float py  = yNorm * h;
        float len = 90.0f + 40.0f * std::sin(phase);

        QLinearGradient sg(QPointF(px, py - len * 0.5f),
                           QPointF(px, py + len * 0.5f));
        sg.setColorAt(0.0, QColor(c.red(), c.green(), c.blue(), 0));
        sg.setColorAt(0.5, QColor(c.red(), c.green(), c.blue(), alpha));
        sg.setColorAt(1.0, QColor(c.red(), c.green(), c.blue(), 0));
        painter.setBrush(sg);
        painter.drawRoundedRect(QRectF(px - 1.5f, py - len * 0.5f, 3.0f, len), 1.5f, 1.5f);
    }

    // ── 4. 粒子 + 连线 ──
    painter.setBrush(Qt::NoBrush);
    // 连线（距离 < 130px）
    for (int i = 0; i < m_particles.size(); ++i) {
        for (int j = i + 1; j < m_particles.size(); ++j) {
            float dx = (m_particles[i].x - m_particles[j].x) * w;
            float dy = (m_particles[i].y - m_particles[j].y) * h;
            float d  = std::sqrt(dx * dx + dy * dy);
            if (d < 130.0f) {
                int la = static_cast<int>((1.0f - d / 130.0f) * 35);
                painter.setPen(QPen(QColor(0, 255, 136, la), 0.7f));
                painter.drawLine(
                    QPointF(m_particles[i].x * w, m_particles[i].y * h),
                    QPointF(m_particles[j].x * w, m_particles[j].y * h));
            }
        }
    }

    // 粒子光晕
    painter.setPen(Qt::NoPen);
    for (const auto& p : m_particles) {
        float alpha = 0.25f + 0.35f * std::sin(m_bgPhase * 2.0f + p.phaseOff);
        int a = static_cast<int>(alpha * 255);
        float px = p.x * w;
        float py = p.y * h;
        float r  = p.radius;

        QRadialGradient pg(QPointF(px, py), r * 4);
        QColor c = p.color;
        c.setAlpha(a);
        pg.setColorAt(0.0, c);
        c.setAlpha(0);
        pg.setColorAt(1.0, c);
        painter.setBrush(pg);
        painter.drawEllipse(QPointF(px, py), r * 4, r * 4);

        // 内核亮点
        c = p.color;
        c.setAlpha(qMin(255, a + 80));
        painter.setBrush(c);
        painter.drawEllipse(QPointF(px, py), r * 0.6f, r * 0.6f);
    }

    // ── 5. 底部频谱条 ──
    painter.setPen(Qt::NoPen);
    float barW   = static_cast<float>(w) / SPECTRUM_BARS;
    float maxBarH = h * 0.11f;
    for (int i = 0; i < SPECTRUM_BARS; ++i) {
        float barH = m_barHeight[i] * maxBarH;
        if (barH < 1.0f) continue;
        float bx = i * barW;
        float by = h - barH;

        // 颜色：绿 → 青 → 紫
        float t = static_cast<float>(i) / (SPECTRUM_BARS - 1);
        int cr, cg, cb;
        if (t < 0.5f) {
            float lt = t * 2.0f;
            cr = static_cast<int>(0   + lt * 0);
            cg = static_cast<int>(255 - lt * 55);
            cb = static_cast<int>(136 + lt * 119);
        } else {
            float lt = (t - 0.5f) * 2.0f;
            cr = static_cast<int>(0   + lt * 189);
            cg = static_cast<int>(200 - lt * 53);
            cb = static_cast<int>(255 - lt * 6);
        }

        QLinearGradient bg(QPointF(bx, h), QPointF(bx, by));
        bg.setColorAt(0.0, QColor(cr, cg, cb, 0));
        bg.setColorAt(0.3, QColor(cr, cg, cb, 130));
        bg.setColorAt(1.0, QColor(cr, cg, cb, 210));
        painter.setBrush(bg);
        painter.drawRoundedRect(QRectF(bx + 1, by, barW - 2, barH), 2, 2);
    }

    // ── 6. 顶部/底部暗角 ──
    QLinearGradient vignette(0, 0, 0, h);
    vignette.setColorAt(0.0,  QColor(0, 0, 0, 100));
    vignette.setColorAt(0.15, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.85, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0,  QColor(0, 0, 0, 120));
    painter.fillRect(rect(), vignette);
}
