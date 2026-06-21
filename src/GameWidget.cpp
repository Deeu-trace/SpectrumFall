#include "GameWidget.h"
#include "AudioEngine.h"
#include "ScoreManager.h"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QApplication>
#include <cmath>

// 判定线和轨道参数常量
static constexpr qreal SCROLL_SPEED = 0.35;     // 像素/毫秒（下落速度，降低让玩家有更多反应时间）
static constexpr qint64 VISIBLE_AHEAD = 3000;   // 提前 3 秒显示音符
static constexpr qint64 MISS_THRESHOLD = 300;   // 超过 300ms 视为 Miss（音符过了判定线后的容许窗口）
static constexpr int LANE_COUNT = 4;
static constexpr int DEFAULT_MIN_GAP_MS = 900;   // 默认同轨道最小间隔 900ms
static constexpr int MIN_GAP_MS = 150;          // 同轨道滑块最小值
static constexpr int MAX_GAP_MS = 1000;         // 同轨道滑块最大值
static constexpr int GAP_STEP_MS = 50;          // 滑块步进

// 跨轨道全局间隔（防止不同轨道音符挤成一团）
static constexpr int DEFAULT_GLOBAL_MIN_GAP_MS = 300;  // 默认跨轨道最小间隔 300ms
static constexpr int GLOBAL_MIN_GAP_MS = 100;          // 全局滑块最小值
static constexpr int GLOBAL_MAX_GAP_MS = 800;          // 全局滑块最大值
static constexpr int GLOBAL_GAP_STEP_MS = 50;          // 全局滑块步进

GameWidget::GameWidget(AudioEngine* audioEngine, ScoreManager* scoreManager, QWidget* parent)
    : QWidget(parent)
    , m_audioEngine(audioEngine)
    , m_scoreManager(scoreManager)
    , m_renderTimer(new QTimer(this))
    , m_paused(false)
    , m_gameActive(false)
    , m_minGapMs(DEFAULT_MIN_GAP_MS)
    , m_globalMinGapMs(DEFAULT_GLOBAL_MIN_GAP_MS)
    , m_densityChanged(false)
    , m_judgeTextTimer(0)
    , m_judgeLineY(0.85)
    , m_noteSpeed(SCROLL_SPEED)
    , m_trackWidth(80)
    , m_startPosMs(0)
    , m_pauseElapsedMs(0)
    , m_totalPausedMs(0)
{
    m_renderTimer->setInterval(16); // ~60Hz

    // 按键状态初始化
    m_keyPressed[0] = false; // D
    m_keyPressed[1] = false; // F
    m_keyPressed[2] = false; // J
    m_keyPressed[3] = false; // K

    setFocusPolicy(Qt::StrongFocus);

    connect(m_renderTimer, &QTimer::timeout, this, &GameWidget::onRenderTick);

    if (m_scoreManager) {
        connect(m_scoreManager, &ScoreManager::scoreChanged, this, [this](int) { update(); });
        connect(m_scoreManager, &ScoreManager::comboChanged, this, [this](int) { update(); });
    }

    setupPauseOverlay();
}

void GameWidget::setupPauseOverlay()
{
    // 全屏半透明遮罩
    m_pauseOverlay = new QWidget(this);
    m_pauseOverlay->setGeometry(0, 0, width(), height());
    m_pauseOverlay->setStyleSheet("background-color: rgba(10, 10, 30, 220);");
    m_pauseOverlay->hide();

    // 主布局：垂直居中
    QVBoxLayout* mainLayout = new QVBoxLayout(m_pauseOverlay);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(16);

    // ── 密度调节区域 ──
    QWidget* densityPanel = new QWidget(m_pauseOverlay);
    densityPanel->setObjectName("controlPanel");
    QVBoxLayout* densityLayout = new QVBoxLayout(densityPanel);
    densityLayout->setAlignment(Qt::AlignCenter);
    densityLayout->setSpacing(8);

    // 标题
    QLabel* densityTitle = new QLabel(QStringLiteral("音符密度调节"), densityPanel);
    densityTitle->setAlignment(Qt::AlignCenter);
    densityTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #00ff88; background: transparent;");
    densityLayout->addWidget(densityTitle);

    // 副标题
    QLabel* densityHint = new QLabel(QStringLiteral("调整后点击「继续游戏」将从头开始"), densityPanel);
    densityHint->setAlignment(Qt::AlignCenter);
    densityHint->setStyleSheet("font-size: 12px; color: #8888aa; background: transparent;");
    densityLayout->addWidget(densityHint);

    // ── 滑块 1: 同轨道间隔 ──
    QLabel* laneGapTitle = new QLabel(QStringLiteral("同轨道最小间隔"), densityPanel);
    laneGapTitle->setAlignment(Qt::AlignCenter);
    laneGapTitle->setStyleSheet("font-size: 13px; color: #bd93f9; background: transparent;");
    densityLayout->addWidget(laneGapTitle);

    // 滑块行
    QHBoxLayout* sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(10);

    QLabel* sparseLabel = new QLabel(QStringLiteral("密集"), densityPanel);
    sparseLabel->setStyleSheet("font-size: 13px; color: #bd93f9; background: transparent;");
    sliderRow->addWidget(sparseLabel);

    m_densitySlider = new QSlider(Qt::Horizontal, densityPanel);
    m_densitySlider->setMinimum(MIN_GAP_MS);
    m_densitySlider->setMaximum(MAX_GAP_MS);
    m_densitySlider->setSingleStep(GAP_STEP_MS);
    m_densitySlider->setPageStep(GAP_STEP_MS * 2);
    m_densitySlider->setValue(m_minGapMs);
    m_densitySlider->setMinimumWidth(200);
    m_densitySlider->setObjectName("densitySlider");
    sliderRow->addWidget(m_densitySlider);

    QLabel* wideLabel = new QLabel(QStringLiteral("稀疏"), densityPanel);
    wideLabel->setStyleSheet("font-size: 13px; color: #bd93f9; background: transparent;");
    sliderRow->addWidget(wideLabel);

    densityLayout->addLayout(sliderRow);

    // 数值显示
    m_densityLabel = new QLabel(QStringLiteral("%1ms").arg(m_minGapMs), densityPanel);
    m_densityLabel->setAlignment(Qt::AlignCenter);
    m_densityLabel->setStyleSheet("font-size: 14px; color: #e0e0e0; background: transparent;");
    densityLayout->addWidget(m_densityLabel);

    // 分隔线
    QFrame* separator = new QFrame(densityPanel);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: #0f3460; background-color: #0f3460; max-height: 1px;");
    densityLayout->addWidget(separator);

    // ── 滑块 2: 跨轨道全局间隔 ──
    QLabel* globalGapTitle = new QLabel(QStringLiteral("跨轨道最小间隔"), densityPanel);
    globalGapTitle->setAlignment(Qt::AlignCenter);
    globalGapTitle->setStyleSheet("font-size: 13px; color: #8be9fd; background: transparent;");
    densityLayout->addWidget(globalGapTitle);

    // 全局滑块行
    QHBoxLayout* globalSliderRow = new QHBoxLayout();
    globalSliderRow->setSpacing(10);

    QLabel* gSparseLabel = new QLabel(QStringLiteral("密集"), densityPanel);
    gSparseLabel->setStyleSheet("font-size: 13px; color: #8be9fd; background: transparent;");
    globalSliderRow->addWidget(gSparseLabel);

    m_globalGapSlider = new QSlider(Qt::Horizontal, densityPanel);
    m_globalGapSlider->setMinimum(GLOBAL_MIN_GAP_MS);
    m_globalGapSlider->setMaximum(GLOBAL_MAX_GAP_MS);
    m_globalGapSlider->setSingleStep(GLOBAL_GAP_STEP_MS);
    m_globalGapSlider->setPageStep(GLOBAL_GAP_STEP_MS * 2);
    m_globalGapSlider->setValue(m_globalMinGapMs);
    m_globalGapSlider->setMinimumWidth(200);
    m_globalGapSlider->setObjectName("globalGapSlider");
    globalSliderRow->addWidget(m_globalGapSlider);

    QLabel* gWideLabel = new QLabel(QStringLiteral("稀疏"), densityPanel);
    gWideLabel->setStyleSheet("font-size: 13px; color: #8be9fd; background: transparent;");
    globalSliderRow->addWidget(gWideLabel);

    densityLayout->addLayout(globalSliderRow);

    // 全局数值显示
    m_globalGapLabel = new QLabel(QStringLiteral("%1ms").arg(m_globalMinGapMs), densityPanel);
    m_globalGapLabel->setAlignment(Qt::AlignCenter);
    m_globalGapLabel->setStyleSheet("font-size: 14px; color: #e0e0e0; background: transparent;");
    densityLayout->addWidget(m_globalGapLabel);

    mainLayout->addWidget(densityPanel);

    // 同轨道滑块变化事件
    connect(m_densitySlider, &QSlider::valueChanged, this, [this](int value) {
        m_densityLabel->setText(QStringLiteral("%1ms").arg(value));
        m_densityChanged = true;
    });

    // 全局滑块变化事件
    connect(m_globalGapSlider, &QSlider::valueChanged, this, [this](int value) {
        m_globalGapLabel->setText(QStringLiteral("%1ms").arg(value));
        m_densityChanged = true;
    });

    // ── 按钮区域 ──
    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->setAlignment(Qt::AlignCenter);
    btnLayout->setSpacing(12);

    // "继续游戏" 按钮
    m_continueBtn = new QPushButton(QStringLiteral("继续游戏"), m_pauseOverlay);
    m_continueBtn->setMinimumSize(200, 50);
    m_continueBtn->setObjectName("menuButton");
    btnLayout->addWidget(m_continueBtn);

    // "返回主菜单" 按钮
    m_backToMenuBtn = new QPushButton(QStringLiteral("返回主菜单"), m_pauseOverlay);
    m_backToMenuBtn->setMinimumSize(200, 50);
    m_backToMenuBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_backToMenuBtn);

    mainLayout->addLayout(btnLayout);

    // 继续游戏
    connect(m_continueBtn, &QPushButton::clicked, this, [this]() {
        int newLaneGap = m_densitySlider->value();
        int newGlobalGap = m_globalGapSlider->value();

        if (m_densityChanged) {
            // 密度被修改 → 重新过滤音符并重新开始
            m_minGapMs = newLaneGap;
            m_globalMinGapMs = newGlobalGap;
            applyDensityFilter();
            m_densityChanged = false;

            // 重新开始当前歌曲（保留密度设置）
            m_paused = false;
            m_gameActive = true;
            m_judgeTextTimer = 0;
            m_judgeText.clear();
            m_startPosMs = 0;
            m_pauseElapsedMs = 0;
            m_totalPausedMs = 0;

            if (m_scoreManager) {
                m_scoreManager->reset();
            }

            m_gameClock.restart();
            m_pauseOverlay->hide();
            setFocus();

            if (m_audioEngine) {
                m_audioEngine->seek(0);
                m_audioEngine->play();
            }
        } else {
            // 密度未修改 → 正常恢复
            resumeGame();
            m_pauseOverlay->hide();
        }
    });

    // 返回主菜单
    connect(m_backToMenuBtn, &QPushButton::clicked, this, [this]() {
        m_gameActive = false;
        m_renderTimer->stop();
        m_pauseOverlay->hide();
        m_audioEngine->pause();
        emit backRequested();
    });
}

void GameWidget::applyDensityFilter()
{
    // 从原始音符列表中，按双重间隔过滤：
    // 1) 同轨道最小间隔 m_minGapMs —— 防止同一轨道连打太快
    // 2) 跨轨道全局最小间隔 m_globalMinGapMs —— 防止不同轨道音符挤成一团
    m_notes.clear();

    // 每个轨道独立追踪最近时间戳
    qint64 lastTimestampPerLane[4] = {-m_minGapMs, -m_minGapMs, -m_minGapMs, -m_minGapMs};
    // 全局最近时间戳（任何轨道产生音符后都更新）
    qint64 lastGlobalTimestamp = -m_globalMinGapMs;

    for (const GameNote& note : m_allNotes) {
        int lane = note.lane;
        if (lane < 0 || lane > 3) lane = 0;

        // 检查 1: 同轨道间隔
        if (note.timestampMs - lastTimestampPerLane[lane] < m_minGapMs) {
            continue;
        }

        // 检查 2: 跨轨道全局间隔
        if (note.timestampMs - lastGlobalTimestamp < m_globalMinGapMs) {
            continue;
        }

        m_notes.append(note);
        lastTimestampPerLane[lane] = note.timestampMs;
        lastGlobalTimestamp = note.timestampMs;
    }
}

void GameWidget::startGame(const QVector<GameNote>& notes)
{
    m_allNotes = notes;  // 保存原始完整音符列表
    m_minGapMs = DEFAULT_MIN_GAP_MS;
    m_globalMinGapMs = DEFAULT_GLOBAL_MIN_GAP_MS;
    m_densityChanged = false;

    // 重置滑块到默认值
    if (m_densitySlider) {
        m_densitySlider->setValue(DEFAULT_MIN_GAP_MS);
    }
    if (m_densityLabel) {
        m_densityLabel->setText(QStringLiteral("%1ms").arg(DEFAULT_MIN_GAP_MS));
    }
    if (m_globalGapSlider) {
        m_globalGapSlider->setValue(DEFAULT_GLOBAL_MIN_GAP_MS);
    }
    if (m_globalGapLabel) {
        m_globalGapLabel->setText(QStringLiteral("%1ms").arg(DEFAULT_GLOBAL_MIN_GAP_MS));
    }

    // 应用密度过滤
    applyDensityFilter();

    m_paused = false;
    m_gameActive = true;
    m_judgeTextTimer = 0;
    m_judgeText.clear();
    m_startPosMs = 0;
    m_pauseElapsedMs = 0;
    m_totalPausedMs = 0;

    // 重置分数
    if (m_scoreManager) {
        m_scoreManager->reset();
    }

    // 启动高精度游戏时钟
    m_gameClock.start();

    setFocus();
    m_renderTimer->start();

    if (m_audioEngine) {
        m_audioEngine->seek(0);
        m_audioEngine->play();
    }
}

void GameWidget::pauseGame()
{
    m_paused = true;
    // 记录暂停时刻已经过的时间
    m_pauseElapsedMs = m_gameClock.elapsed();
    if (m_audioEngine) {
        m_audioEngine->pause();
    }
}

void GameWidget::resumeGame()
{
    m_paused = false;
    // 累加暂停时间
    m_totalPausedMs += (m_gameClock.elapsed() - m_pauseElapsedMs);
    setFocus();
    if (m_audioEngine) {
        m_audioEngine->play();
    }
}

void GameWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_pauseOverlay) {
        m_pauseOverlay->setGeometry(0, 0, width(), height());
    }
}

qint64 GameWidget::getGameTime() const
{
    if (!m_gameClock.isValid()) return 0;
    qint64 elapsed = m_gameClock.elapsed();
    // 减去累计暂停时间，得到实际游戏时间
    qint64 gameTime = elapsed - m_totalPausedMs + m_startPosMs;
    return qMax<qint64>(0, gameTime);
}

void GameWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // 背景
    painter.fillRect(rect(), QColor(20, 20, 40));

    // 计算轨道参数
    m_trackWidth = qMax(60.0, w * 0.12);
    qreal totalTrackWidth = m_trackWidth * LANE_COUNT;
    qreal startX = (w - totalTrackWidth) / 2.0;
    qreal judgeY = h * m_judgeLineY;

    // 绘制轨道背景
    for (int i = 0; i < LANE_COUNT; ++i) {
        qreal x = startX + i * m_trackWidth;
        QColor trackBg = (i % 2 == 0) ? QColor(30, 30, 55, 100) : QColor(25, 25, 50, 100);
        painter.fillRect(QRectF(x, 0, m_trackWidth, h), trackBg);

        // 轨道边线
        painter.setPen(QColor(60, 60, 100, 80));
        painter.drawLine(QPointF(x, 0), QPointF(x, h));
    }
    // 最后一根边线
    painter.setPen(QColor(60, 60, 100, 80));
    painter.drawLine(QPointF(startX + totalTrackWidth, 0), QPointF(startX + totalTrackWidth, h));

    // 绘制判定线（发光效果）
    QLinearGradient judgeGlow(startX, judgeY - 15, startX, judgeY + 15);
    judgeGlow.setColorAt(0.0, QColor(0, 255, 136, 0));
    judgeGlow.setColorAt(0.4, QColor(0, 255, 136, 100));
    judgeGlow.setColorAt(0.5, QColor(0, 255, 136, 200));
    judgeGlow.setColorAt(0.6, QColor(0, 255, 136, 100));
    judgeGlow.setColorAt(1.0, QColor(0, 255, 136, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(judgeGlow);
    painter.drawRect(QRectF(startX, judgeY - 15, totalTrackWidth, 30));

    // 判定线主线
    painter.setPen(QPen(QColor(0, 255, 136, 255), 2));
    painter.drawLine(QPointF(startX, judgeY), QPointF(startX + totalTrackWidth, judgeY));

    // 获取精确游戏时间（用于音符位置）
    qint64 currentPos = getGameTime();

    // 绘制音符
    if (m_gameActive) {
        qreal noteHeight = 12;

        for (const GameNote& note : m_notes) {
            if (note.judged) continue;

            // 计算音符 Y 位置：正值表示音符在判定线上方，负值表示已过判定线
            qint64 timeDelta = note.timestampMs - currentPos;
            qreal noteY = judgeY - timeDelta * m_noteSpeed;

            // 仅绘制可见范围内的音符
            if (noteY < -noteHeight * 2 || noteY > h + noteHeight) {
                continue;
            }

            qreal noteX = startX + note.lane * m_trackWidth + 4;
            qreal noteW = m_trackWidth - 8;

            // 音符颜色
            QColor color = laneColor(note.lane);

            // 按键高亮
            if (m_keyPressed.value(note.lane, false)) {
                color = color.lighter(150);
            }

            // 绘制音符
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(noteX, noteY - noteHeight / 2, noteW, noteHeight), 4, 4);

            // 音符辉光
            QRadialGradient noteGlow(QPointF(noteX + noteW / 2, noteY), noteW * 0.6);
            noteGlow.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 60));
            noteGlow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
            painter.setBrush(noteGlow);
            painter.drawEllipse(QPointF(noteX + noteW / 2, noteY), noteW * 0.6, noteHeight * 2);
        }
    }

    // 按键按下时轨道底部高亮
    for (int i = 0; i < LANE_COUNT; ++i) {
        if (m_keyPressed.value(i, false)) {
            qreal x = startX + i * m_trackWidth;
            QColor color = laneColor(i);
            QLinearGradient keyGlow(x, judgeY - 30, x, judgeY + 10);
            keyGlow.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 0));
            keyGlow.setColorAt(0.7, QColor(color.red(), color.green(), color.blue(), 80));
            keyGlow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 150));
            painter.setPen(Qt::NoPen);
            painter.setBrush(keyGlow);
            painter.drawRect(QRectF(x, judgeY - 30, m_trackWidth, 40));
        }
    }

    // 绘制判定文字动画
    if (m_judgeTextTimer > 0 && !m_judgeText.isEmpty()) {
        qreal textX = startX + (m_judgeTextLane + 0.5) * m_trackWidth;
        qreal textY = judgeY - 60;

        QFont judgeFont;
        judgeFont.setPixelSize(24);
        judgeFont.setBold(true);
        painter.setFont(judgeFont);
        painter.setPen(m_judgeTextColor);

        // 淡出效果
        int alpha = qMin(255, m_judgeTextTimer * 25);
        QColor textColor = m_judgeTextColor;
        textColor.setAlpha(alpha);
        painter.setPen(textColor);
        painter.drawText(QRectF(textX - 60, textY - 15, 120, 30),
                         Qt::AlignCenter, m_judgeText);
    }

    // HUD：分数和连击
    QFont hudFont;
    hudFont.setPixelSize(20);
    painter.setFont(hudFont);

    // 分数
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(15, 30, QStringLiteral("分数: %1").arg(
        m_scoreManager ? m_scoreManager->score() : 0));

    // 连击
    painter.setPen(QColor(0, 255, 136));
    hudFont.setPixelSize(28);
    hudFont.setBold(true);
    painter.setFont(hudFont);
    int combo = m_scoreManager ? m_scoreManager->combo() : 0;
    if (combo > 0) {
        painter.drawText(QRectF(0, 50, w, 40), Qt::AlignCenter,
                         QStringLiteral("%1 Combo").arg(combo));
    }

    // 底部按键提示 + 当前密度
    hudFont.setPixelSize(14);
    hudFont.setBold(false);
    painter.setFont(hudFont);
    painter.setPen(QColor(120, 120, 160));
    const QString keys[] = {"D", "F", "J", "K"};
    for (int i = 0; i < LANE_COUNT; ++i) {
        qreal x = startX + i * m_trackWidth;
        painter.drawText(QRectF(x, h - 25, m_trackWidth, 20), Qt::AlignCenter, keys[i]);
    }

    // 右下角显示当前密度
    painter.setPen(QColor(80, 80, 120));
    hudFont.setPixelSize(12);
    painter.setFont(hudFont);
    painter.drawText(w - 220, h - 10, QStringLiteral("同轨: %1ms | 跨轨: %2ms | ESC 暂停")
                     .arg(m_minGapMs).arg(m_globalMinGapMs));
}

void GameWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    int lane = -1;
    switch (event->key()) {
    case Qt::Key_D: lane = 0; break;
    case Qt::Key_F: lane = 1; break;
    case Qt::Key_J: lane = 2; break;
    case Qt::Key_K: lane = 3; break;
    case Qt::Key_Escape:
        if (!m_gameActive) {
            return;
        }
        if (m_paused) {
            resumeGame();
            m_pauseOverlay->hide();
        } else {
            pauseGame();
            m_pauseOverlay->setGeometry(0, 0, width(), height());
            m_pauseOverlay->show();
            m_continueBtn->setFocus();
        }
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    if (lane >= 0) {
        m_keyPressed[lane] = true;
        if (m_gameActive && !m_paused) {
            judgeLane(lane);
        }
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    int lane = -1;
    switch (event->key()) {
    case Qt::Key_D: lane = 0; break;
    case Qt::Key_F: lane = 1; break;
    case Qt::Key_J: lane = 2; break;
    case Qt::Key_K: lane = 3; break;
    default:
        QWidget::keyReleaseEvent(event);
        return;
    }

    if (lane >= 0) {
        m_keyPressed[lane] = false;
    }
}

void GameWidget::onRenderTick()
{
    if (!m_gameActive || m_paused) return;

    // 检查超时未击打的音符
    checkMissedNotes();

    // 判定文字动画倒计时
    if (m_judgeTextTimer > 0) {
        m_judgeTextTimer--;
    }

    // 检查游戏是否结束
    qint64 gameTime = getGameTime();
    if (m_audioEngine) {
        qint64 dur = m_audioEngine->duration();
        if (gameTime >= dur && dur > 0) {
            // 等待所有音符判定完毕
            bool allJudged = true;
            for (const GameNote& note : m_notes) {
                if (!note.judged) {
                    allJudged = false;
                    break;
                }
            }
            if (allJudged) {
                m_gameActive = false;
                m_renderTimer->stop();
                m_audioEngine->pause();
                emit gameFinished();
                return;
            }
        }
    }

    update();
}

void GameWidget::checkMissedNotes()
{
    qint64 currentPos = getGameTime();

    for (GameNote& note : m_notes) {
        if (note.judged) continue;

        // 音符超过判定线 + MISS_THRESHOLD 毫秒
        if (currentPos - note.timestampMs > MISS_THRESHOLD) {
            note.judged = true;
            note.judgment = 3; // Miss

            if (m_scoreManager) {
                m_scoreManager->addMiss();
            }

            // 显示 Miss 文字
            m_judgeText = QStringLiteral("Miss");
            m_judgeTextLane = note.lane;
            m_judgeTextColor = QColor(233, 69, 96);
            m_judgeTextTimer = 10;
        }
    }
}

void GameWidget::judgeLane(int lane)
{
    if (!m_scoreManager) return;

    // 使用高精度游戏时钟作为判定时间（不再依赖 QMediaPlayer::position()）
    qint64 hitTime = getGameTime();

    // 找到该轨道最近的未判定音符
    qint64 minDelta = INT64_MAX;
    int bestIdx = -1;

    for (int i = 0; i < m_notes.size(); ++i) {
        GameNote& note = m_notes[i];
        if (note.judged || note.lane != lane) continue;

        qint64 delta = qAbs(hitTime - note.timestampMs);
        if (delta < minDelta) {
            minDelta = delta;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) return;

    // 判定
    int result = m_scoreManager->judgeHit(hitTime, m_notes[bestIdx].timestampMs);
    m_notes[bestIdx].judged = true;
    m_notes[bestIdx].judgment = result;

    // 显示判定文字
    if (result == 1) {
        m_judgeText = QStringLiteral("Perfect");
        m_judgeTextColor = QColor(0, 255, 136);
    } else if (result == 2) {
        m_judgeText = QStringLiteral("Good");
        m_judgeTextColor = QColor(255, 255, 100);
    } else {
        m_judgeText = QStringLiteral("Miss");
        m_judgeTextColor = QColor(233, 69, 96);
    }
    m_judgeTextLane = lane;
    m_judgeTextTimer = 10;
}

QColor GameWidget::laneColor(int lane) const
{
    switch (lane) {
    case 0: return QColor(0, 255, 136);     // D: 荧光绿
    case 1: return QColor(189, 147, 249);    // F: 浅紫
    case 2: return QColor(139, 233, 253);    // J: 浅蓝
    case 3: return QColor(255, 121, 198);    // K: 粉色
    default: return QColor(255, 255, 255);
    }
}

qreal GameWidget::laneX(int lane) const
{
    qreal totalTrackWidth = m_trackWidth * LANE_COUNT;
    qreal startX = (width() - totalTrackWidth) / 2.0;
    return startX + lane * m_trackWidth;
}
