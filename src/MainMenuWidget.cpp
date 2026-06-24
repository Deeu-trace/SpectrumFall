#include "MainMenuWidget.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <cmath>

MainMenuWidget::MainMenuWidget(QWidget* parent)
    : QWidget(parent)
    , m_bgPhase(0.0f)
{
    // 主布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    // 弹簧
    layout->addStretch(3);

    // 标题（用 QLabel）
    QLabel* titleLabel = new QLabel(QStringLiteral("SpectrumFall"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(64);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // 副标题
    QLabel* subtitleLabel = new QLabel(QStringLiteral("Audio Visualization & Rhythm Game"), this);
    QFont subFont = subtitleLabel->font();
    subFont.setPixelSize(18);
    subtitleLabel->setFont(subFont);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("color: #bd93f9; background: transparent;");
    layout->addWidget(subtitleLabel);

    layout->addSpacing(40);

    // 选择歌曲按钮
    m_selectSongBtn = new QPushButton(QStringLiteral("选择歌曲"), this);
    m_selectSongBtn->setMinimumSize(220, 50);
    m_selectSongBtn->setObjectName("menuButton");
    layout->addWidget(m_selectSongBtn, 0, Qt::AlignCenter);

    layout->addSpacing(10);

    // 排行榜按钮
    m_leaderboardBtn = new QPushButton(QStringLiteral("排行榜"), this);
    m_leaderboardBtn->setMinimumSize(220, 50);
    m_leaderboardBtn->setObjectName("menuButton");
    layout->addWidget(m_leaderboardBtn, 0, Qt::AlignCenter);

    layout->addSpacing(10);

    // 退出按钮
    m_exitBtn = new QPushButton(QStringLiteral("退出"), this);
    m_exitBtn->setMinimumSize(220, 50);
    m_exitBtn->setObjectName("menuButton");
    layout->addWidget(m_exitBtn, 0, Qt::AlignCenter);

    layout->addStretch(2);

    // 连接信号
    connect(m_selectSongBtn, &QPushButton::clicked, this, &MainMenuWidget::songSelectRequested);
    connect(m_leaderboardBtn, &QPushButton::clicked, this, &MainMenuWidget::leaderboardRequested);
    connect(m_exitBtn, &QPushButton::clicked, this, &MainMenuWidget::exitRequested);

    // 背景动画定时器
    QTimer* animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, [this]() {
        m_bgPhase += 0.02f;
        if (m_bgPhase > 2.0f * static_cast<float>(M_PI)) m_bgPhase -= 2.0f * static_cast<float>(M_PI);
        update();
    });
    animTimer->start(33); // ~30fps 背景动画
}

void MainMenuWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 深色背景
    painter.fillRect(rect(), QColor(26, 26, 46));

    // 背景装饰：浮动的圆形光晕
    int w = width();
    int h = height();

    for (int i = 0; i < 5; ++i) {
        float phase = m_bgPhase + i * 1.2f;
        float x = w * 0.5f + std::sin(phase) * w * 0.3f;
        float y = h * 0.5f + std::cos(phase * 0.7f + i) * h * 0.25f;
        float radius = 60.0f + std::sin(phase * 0.5f) * 30.0f;

        QRadialGradient glow(QPointF(x, y), radius);
        if (i % 2 == 0) {
            glow.setColorAt(0.0, QColor(0, 255, 136, 25));
            glow.setColorAt(1.0, QColor(0, 255, 136, 0));
        } else {
            glow.setColorAt(0.0, QColor(189, 147, 249, 20));
            glow.setColorAt(1.0, QColor(189, 147, 249, 0));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(QPointF(x, y), radius, radius);
    }
}
