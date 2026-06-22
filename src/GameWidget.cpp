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
#include <QDateTime>
#include <QtMath>
#include <cmath>
#include <random>
#include <algorithm>
#include <QPixmap>

// 判定线和轨道参数常量
static constexpr qreal SCROLL_SPEED = 0.35;     // 像素/毫秒（下落速度，降低让玩家有更多反应时间）
static constexpr qint64 VISIBLE_AHEAD = 3000;   // 提前 3 秒显示音符
static constexpr qint64 MISS_THRESHOLD = 300;   // 超过 300ms 视为 Miss（音符过了判定线后的容许窗口）
static constexpr int DEFAULT_MIN_GAP_MS = 500;   // 默认同轨道最小间隔 500ms（适中密度）
static constexpr int MIN_GAP_MS = 150;          // 同轨道滑块最小值
static constexpr int MAX_GAP_MS = 1000;         // 同轨道滑块最大值
static constexpr int GAP_STEP_MS = 50;          // 滑块步进

// 跨轨道全局间隔（防止不同轨道音符挤成一团）
static constexpr int DEFAULT_GLOBAL_MIN_GAP_MS = 200;  // 默认跨轨道最小间隔 200ms（适中）
static constexpr int GLOBAL_MIN_GAP_MS = 100;          // 全局滑块最小值
static constexpr int GLOBAL_MAX_GAP_MS = 800;          // 全局滑块最大值
static constexpr int GLOBAL_GAP_STEP_MS = 50;          // 全局滑块步进

// Hold 合并阈值：同轨道相邻音符间距 ≤ 此值时合并为长条
// 6 键模式音符更分散，阈值更大才能产生足够 Hold
static constexpr qint64 HOLD_MERGE_THRESHOLD_4K = 500;
static constexpr qint64 HOLD_MERGE_THRESHOLD_6K = 800;

static float randFloat() {
    // 使用 C++ 标准库的随机数生成替代 qrand
    static thread_local std::mt19937 generator(static_cast<unsigned int>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff));
    static thread_local std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}

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
    , m_laneCount(6)
    , m_startPosMs(0)
    , m_pauseElapsedMs(0)
    , m_totalPausedMs(0)
    , m_judgeLinePulse(0.0)
    , m_comboScale(1.0)
    , m_lastCombo(0)
    , m_lastFrameTime(0)
    , m_holdSparkleCounter(0)
{
    m_renderTimer->setInterval(16); // ~60Hz

    // 按键状态初始化（6 键最大）
    m_keyPressed[0] = false; // S
    m_keyPressed[1] = false; // D
    m_keyPressed[2] = false; // F
    m_keyPressed[3] = false; // J
    m_keyPressed[4] = false; // K
    m_keyPressed[5] = false; // L

    setFocusPolicy(Qt::StrongFocus);

    connect(m_renderTimer, &QTimer::timeout, this, &GameWidget::onRenderTick);

    if (m_scoreManager) {
        // 渲染定时器已经以 60Hz 调用 update()，不需要额外触发
    }

    generateStars();
    setupPauseOverlay();
}

void GameWidget::generateStars()
{
    m_stars.clear();
    m_starPhases.clear();
    // qsrand(QDateTime::currentMSecsSinceEpoch()); // 移除未定义的 qsrand
    for (int i = 0; i < 50; ++i) {
        m_stars.append(QPointF(randFloat(), randFloat()));
        m_starPhases.append(randFloat() * 6.28318f);
    }
}

void GameWidget::rebuildBackground()
{
    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) return;

    m_bgCache = QPixmap(w, h);
    m_bgCacheSize = QSize(w, h);

    QPainter p(&m_bgCache);
    p.setRenderHint(QPainter::Antialiasing);

    // 深色径向渐变背景
    QRadialGradient bgGrad(w / 2.0, h / 2.0, w * 0.7);
    bgGrad.setColorAt(0.0, QColor(18, 16, 45));
    bgGrad.setColorAt(1.0, QColor(5, 5, 18));
    p.fillRect(m_bgCache.rect(), bgGrad);

    // 静态星光（不闪烁的基础层）
    p.setPen(Qt::NoPen);
    for (int i = 0; i < m_stars.size(); ++i) {
        int sz = (i % 5 == 0) ? 2 : 1;
        p.setBrush(QColor(140, 180, 255, 80));
        p.drawEllipse(QPointF(m_stars[i].x() * w, m_stars[i].y() * h), sz, sz);
    }
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
            m_activeHolds.clear();
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
    QVector<qint64> lastTimestampPerLane(m_laneCount, -m_minGapMs);
    // 全局最近时间戳（任何轨道产生音符后都更新）
    qint64 lastGlobalTimestamp = -m_globalMinGapMs;

    for (const GameNote& note : m_allNotes) {
        int lane = note.lane;
        if (lane < 0 || lane >= m_laneCount) lane = 1;

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

    // 将密集的同轨道音符合并为 Hold 长条
    mergeHolds();
}

void GameWidget::mergeHolds()
{
    // 根据轨道数选择合并阈值
    qint64 holdThreshold = (m_laneCount == 6) ? HOLD_MERGE_THRESHOLD_6K : HOLD_MERGE_THRESHOLD_4K;

    // 按轨道分组，记录每条轨道上的音符在 m_notes 中的索引
    QMap<int, QVector<int>> laneIndices;
    for (int i = 0; i < m_notes.size(); ++i) {
        laneIndices[m_notes[i].lane].append(i);
    }

    // 标记被合并掉的音符
    QVector<bool> consumed(m_notes.size(), false);
    QVector<GameNote> holdNotes;

    for (auto it = laneIndices.begin(); it != laneIndices.end(); ++it) {
        const QVector<int>& indices = it.value();

        for (int j = 0; j < indices.size(); ++j) {
            if (consumed[indices[j]]) continue;

            int startIdx = indices[j];
            qint64 startTime = m_notes[startIdx].timestampMs;
            qint64 endTime = startTime;

            // 尝试与后续同轨道音符合并
            int k = j + 1;
            while (k < indices.size()) {
                qint64 nextTime = m_notes[indices[k]].timestampMs;
                if (nextTime - endTime <= holdThreshold) {
                    endTime = nextTime;
                    consumed[indices[k]] = true;
                    k++;
                } else {
                    break;
                }
            }

            if (endTime > startTime) {
                // 合并为 Hold 音符
                holdNotes.append(GameNote(startTime, it.key(), HOLD, endTime - startTime));
                consumed[startIdx] = true;
            }
        }
    }

    // 收集未被合并的 Tap 音符 + 新生成的 Hold 音符
    QVector<GameNote> result;
    for (int i = 0; i < m_notes.size(); ++i) {
        if (!consumed[i]) {
            result.append(m_notes[i]);
        }
    }
    for (const GameNote& hn : holdNotes) {
        result.append(hn);
    }

    // 按时间戳排序
    std::sort(result.begin(), result.end(), [](const GameNote& a, const GameNote& b) {
        return a.timestampMs < b.timestampMs;
    });

    m_notes = result;
}

void GameWidget::startGame(const QVector<GameNote>& notes, int laneCount)
{
    m_laneCount = qBound(4, laneCount, 6);
    m_allNotes = notes;
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
    m_judgeLinePulse = 0.0;
    m_comboScale = 1.0;
    m_lastCombo = 0;
    m_lastFrameTime = 0;
    m_particles.clear();
    m_rings.clear();
    m_popups.clear();
    m_activeHolds.clear();
    m_holdSparkleCounter = 0;

    // 重置分数
    if (m_scoreManager) {
        m_scoreManager->reset();
    }

    // 启动高精度游戏时钟
    m_gameClock.start();

    setFocus();
    m_renderTimer->start();

    // 预建背景缓存
    rebuildBackground();

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
    // 标记背景缓存需要重建（paintEvent 中会检测并重建）
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
    qint64 gameTime = getGameTime();
    float timeSec = gameTime / 1000.0f;

    // ═══ 1. 背景（缓存层 + 动态星光层）═══
    // 如果缓存尺寸不匹配，重建
    if (m_bgCacheSize != QSize(w, h)) {
        rebuildBackground();
    }

    // 绘制缓存的静态背景
    painter.drawPixmap(0, 0, m_bgCache);

    // 动态闪烁星光（只画大星的十字光芒，减少计算量）
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < m_stars.size(); ++i) {
        if (i % 7 != 0) continue;  // 只画大星
        float twinkle = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(timeSec * (0.8f + m_starPhases[i] * 0.3f) + m_starPhases[i]));
        if (twinkle <= 0.6f) continue;
        int alpha = static_cast<int>(120 * twinkle);
        qreal sx = m_stars[i].x() * w;
        qreal sy = m_stars[i].y() * h;
        painter.setBrush(QColor(140, 180, 255, alpha));
        painter.drawEllipse(QPointF(sx, sy), 2, 2);
        painter.setPen(QPen(QColor(140, 180, 255, alpha / 2), 0.5));
        painter.drawLine(QPointF(sx - 4, sy), QPointF(sx + 4, sy));
        painter.drawLine(QPointF(sx, sy - 4), QPointF(sx, sy + 4));
        painter.setPen(Qt::NoPen);
    }

    // 低音波纹（从判定线中心扩散）
    for (int r = 0; r < 3; ++r) {
        float waveTime = std::fmod(timeSec * 0.4f + r * 0.333f, 1.0f);
        qreal waveR = waveTime * w * 0.5;
        int waveAlpha = static_cast<int>(60 * (1.0f - waveTime) * (waveTime > 0.05f ? 1.0f : waveTime / 0.05f));
        painter.setBrush(QColor(30, 80, 150, waveAlpha));
        painter.drawEllipse(QPointF(w / 2.0, h * m_judgeLineY), waveR, waveR * 0.3);
    }

    // ═══ 2. 轨道参数 ═══
    int N = m_laneCount;
    m_trackWidth = qMax(45.0, w * (N == 4 ? 0.12 : 0.09));
    qreal totalTrackWidth = m_trackWidth * N;
    qreal startX = (w - totalTrackWidth) / 2.0;
    qreal judgeY = h * m_judgeLineY;

    // ═══ 3. 轨道灯带 ═══
    for (int i = 0; i < N; ++i) {
        qreal x = startX + i * m_trackWidth;
        QColor lc = laneColor(i);

        // 轨道背景（交替深色）
        QColor trackBg = (i % 2 == 0) ? QColor(25, 22, 50, 80) : QColor(20, 18, 42, 80);
        painter.fillRect(QRectF(x, 0, m_trackWidth, h), trackBg);

        // 左侧灯带
        QLinearGradient leftGlow(x, 0, x + 6, 0);
        leftGlow.setColorAt(0.0, QColor(lc.red(), lc.green(), lc.blue(), 60));
        leftGlow.setColorAt(1.0, QColor(lc.red(), lc.green(), lc.blue(), 0));
        painter.fillRect(QRectF(x, 0, 6, h), leftGlow);

        // 右侧灯带
        QLinearGradient rightGlow(x + m_trackWidth - 6, 0, x + m_trackWidth, 0);
        rightGlow.setColorAt(0.0, QColor(lc.red(), lc.green(), lc.blue(), 0));
        rightGlow.setColorAt(1.0, QColor(lc.red(), lc.green(), lc.blue(), 60));
        painter.fillRect(QRectF(x + m_trackWidth - 6, 0, 6, h), rightGlow);

        // 按键时灯带加亮
        if (m_keyPressed.value(i, false)) {
            QLinearGradient pressGlow(x, 0, x + m_trackWidth, 0);
            pressGlow.setColorAt(0.0, QColor(lc.red(), lc.green(), lc.blue(), 0));
            pressGlow.setColorAt(0.5, QColor(lc.red(), lc.green(), lc.blue(), 30));
            pressGlow.setColorAt(1.0, QColor(lc.red(), lc.green(), lc.blue(), 0));
            painter.fillRect(QRectF(x, 0, m_trackWidth, h), pressGlow);
        }
    }

    // 轨道边线
    for (int i = 0; i <= N; ++i) {
        qreal x = startX + i * m_trackWidth;
        painter.setPen(QPen(QColor(60, 60, 100, 60), 1));
        painter.drawLine(QPointF(x, 0), QPointF(x, h));
    }

    // ═══ 4. 音符（霓虹化）═══
    if (m_gameActive) {
        qreal noteHeight = 14;

        // 只遍历可见时间窗口内的音符（优化：跳过太早和太晚的音符）
        qint64 earliestTime = gameTime - MISS_THRESHOLD - 200;  // 刚过判定线的
        qint64 latestTime = gameTime + VISIBLE_AHEAD;           // 还没出现的

        for (const GameNote& note : m_notes) {
            if (note.judged) continue;
            if (note.timestampMs > latestTime) break;  // 后面的更晚，直接退出
            // Hold 的尾部可能还在屏幕上，需要检查 holdEndMs
            if (note.noteType == HOLD) {
                if (note.holdEndMs() < earliestTime) continue;
            } else {
                if (note.timestampMs < earliestTime) continue;
            }

            qint64 timeDelta = note.timestampMs - gameTime;
            qreal headY = judgeY - timeDelta * m_noteSpeed;

            if (note.noteType == HOLD && note.holdDurationMs > 0) {
                // ── Hold 长条渲染 ──
                qint64 tailTimeDelta = note.timestampMs + note.holdDurationMs - gameTime;
                qreal tailY = judgeY - tailTimeDelta * m_noteSpeed;

                // 活跃 Hold：头部固定在判定线，长条从 tailY 到 judgeY
                qreal barTopY = qMin(headY, tailY);
                qreal barBottomY;
                if (note.holdActive) {
                    barBottomY = judgeY;
                } else {
                    barBottomY = qMax(headY, tailY);
                }

                // 可见性检查
                if (barTopY > h || barBottomY < 0) continue;

                qreal noteX = startX + note.lane * m_trackWidth + 4;
                qreal noteW = m_trackWidth - 8;
                QColor color = laneColor(note.lane);

                // 长条主体（半透明渐变）
                QLinearGradient barFill(noteX, barTopY, noteX, barBottomY);
                if (note.holdActive) {
                    // 活跃状态：更亮
                    barFill.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 80));
                    barFill.setColorAt(0.5, QColor(color.red(), color.green(), color.blue(), 160));
                    barFill.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 200));
                } else {
                    barFill.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 50));
                    barFill.setColorAt(0.5, QColor(color.red(), color.green(), color.blue(), 100));
                    barFill.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 150));
                }
                painter.setBrush(barFill);
                painter.setPen(QPen(color.lighter(140), 1.0));
                qreal barH = barBottomY - barTopY;
                if (barH > 0) {
                    painter.drawRoundedRect(QRectF(noteX, barTopY, noteW, barH), 4, 4);
                }

                // 头部圆角（如果未活跃且在可见范围内）
                if (!note.holdActive && headY > -noteHeight && headY < h + noteHeight) {
                    QLinearGradient headFill(noteX, headY - noteHeight / 2, noteX, headY + noteHeight / 2);
                    headFill.setColorAt(0.0, color.lighter(140));
                    headFill.setColorAt(0.5, color);
                    headFill.setColorAt(1.0, color.darker(160));
                    painter.setBrush(headFill);
                    painter.setPen(QPen(color.lighter(180), 1.5));
                    painter.drawRoundedRect(QRectF(noteX, headY - noteHeight / 2, noteW, noteHeight), 5, 5);
                }

                // 尾部指示线（如果可见）
                if (tailY > 0 && tailY < h) {
                    painter.setPen(QPen(color.lighter(180), 2));
                    painter.drawLine(QPointF(noteX, tailY), QPointF(noteX + noteW, tailY));
                }

                // 活跃 Hold：判定线位置脉动光晕
                if (note.holdActive) {
                    float pulse = 0.6f + 0.4f * std::sin(timeSec * 8.0f);
                    qreal glowR = m_trackWidth * 0.6 * pulse;
                    QRadialGradient holdGlow(noteX + noteW / 2.0, judgeY, glowR);
                    holdGlow.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), static_cast<int>(180 * pulse)));
                    holdGlow.setColorAt(0.5, QColor(color.red(), color.green(), color.blue(), static_cast<int>(80 * pulse)));
                    holdGlow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(holdGlow);
                    painter.drawEllipse(QPointF(noteX + noteW / 2.0, judgeY), glowR, glowR * 0.5);
                }
            } else {
                // ── 普通 Tap 音符渲染 ──
                if (headY < -noteHeight * 2 || headY > h + noteHeight) continue;

                qreal noteX = startX + note.lane * m_trackWidth + 4;
                qreal noteW = m_trackWidth - 8;
                QColor color = laneColor(note.lane);

                if (m_keyPressed.value(note.lane, false)) {
                    color = color.lighter(160);
                }

                // 音符主体（渐变填充）
                QLinearGradient noteFill(noteX, headY - noteHeight / 2, noteX, headY + noteHeight / 2);
                noteFill.setColorAt(0.0, color.lighter(140));
                noteFill.setColorAt(0.5, color);
                noteFill.setColorAt(1.0, color.darker(160));
                painter.setBrush(noteFill);
                painter.setPen(QPen(color.lighter(180), 1.5));
                painter.drawRoundedRect(QRectF(noteX, headY - noteHeight / 2, noteW, noteHeight), 5, 5);
            }
        }
    }

    // ═══ 5. 判定线 ═══
    qreal glowH = 18;

    QLinearGradient judgeGlow(startX, judgeY - glowH, startX, judgeY + glowH);
    judgeGlow.setColorAt(0.0, QColor(0, 255, 136, 0));
    judgeGlow.setColorAt(0.4, QColor(0, 255, 136, 80));
    judgeGlow.setColorAt(0.5, QColor(0, 255, 136, 220));
    judgeGlow.setColorAt(0.6, QColor(0, 255, 136, 80));
    judgeGlow.setColorAt(1.0, QColor(0, 255, 136, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(judgeGlow);
    painter.drawRect(QRectF(startX, judgeY - glowH, totalTrackWidth, glowH * 2));

    // 判定线主线
    painter.setPen(QPen(QColor(0, 255, 136, 255), 2));
    painter.drawLine(QPointF(startX, judgeY), QPointF(startX + totalTrackWidth, judgeY));

    // 按键时轨道底部高亮
    for (int i = 0; i < N; ++i) {
        if (m_keyPressed.value(i, false)) {
            qreal x = startX + i * m_trackWidth;
            QColor color = laneColor(i);
            QLinearGradient keyGlow(x, judgeY - 40, x, judgeY + 10);
            keyGlow.setColorAt(0.0, QColor(color.red(), color.green(), color.blue(), 0));
            keyGlow.setColorAt(0.7, QColor(color.red(), color.green(), color.blue(), 100));
            keyGlow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 180));
            painter.setPen(Qt::NoPen);
            painter.setBrush(keyGlow);
            painter.drawRect(QRectF(x, judgeY - 40, m_trackWidth, 50));
        }
    }

    // ═══ 6. 命中反馈环 ═══
    painter.setPen(Qt::NoPen);
    for (const HitRing& ring : m_rings) {
        float t = static_cast<float>(ring.age) / ring.maxAge;
        qreal radius = ring.startRadius + (ring.endRadius - ring.startRadius) * t;
        int alpha = static_cast<int>(200 * (1.0 - t));
        painter.setBrush(QColor(ring.color.red(), ring.color.green(), ring.color.blue(), alpha));
        painter.drawEllipse(QPointF(ring.x, ring.y), radius, radius);
    }

    // ═══ 7. 命中粒子 ═══
    for (const HitParticle& p : m_particles) {
        float t = static_cast<float>(p.age) / p.maxAge;
        int alpha = static_cast<int>(255 * (1.0 - t));
        painter.setBrush(QColor(p.color.red(), p.color.green(), p.color.blue(), alpha));
        painter.drawEllipse(QPointF(p.x, p.y), p.size * (1.0 - t * 0.5), p.size * (1.0 - t * 0.5));
    }

    // ═══ 8. 判定文字动画（弹跳）═══
    if (m_judgeTextTimer > 0 && !m_judgeText.isEmpty()) {
        qreal textX = startX + (m_judgeTextLane + 0.5) * m_trackWidth;
        qreal textY = judgeY - 70;

        float textProgress = 1.0f - static_cast<float>(m_judgeTextTimer) / 10.0f;
        qreal scale = 1.0 + (1.0 - textProgress) * 0.5;  // 从 1.5 缩到 1.0
        int alpha = qMin(255, m_judgeTextTimer * 30);

        QFont judgeFont;
        judgeFont.setPixelSize(static_cast<int>(22 * scale));
        judgeFont.setBold(true);
        painter.setFont(judgeFont);
        QColor textColor = m_judgeTextColor;
        textColor.setAlpha(alpha);
        painter.setPen(textColor);
        painter.drawText(QRectF(textX - 80, textY - 20, 160, 40),
                         Qt::AlignCenter, m_judgeText);
    }

    // ═══ 9. 分数弹出动画 ═══
    for (const ScorePopup& popup : m_popups) {
        float t = static_cast<float>(popup.age) / popup.maxAge;
        qreal yOffset = -40.0 * t;
        int alpha = static_cast<int>(255 * (1.0 - t));
        QFont popFont;
        popFont.setPixelSize(static_cast<int>(16 + 8 * (1.0 - t)));
        popFont.setBold(true);
        painter.setFont(popFont);
        QColor c = popup.color;
        c.setAlpha(alpha);
        painter.setPen(c);
        painter.drawText(QRectF(popup.x - 60, popup.y + yOffset - 15, 120, 30),
                         Qt::AlignCenter, popup.text);
    }

    // ═══ 10. HUD：进度条 + 分数和连击 ═══
    // 顶部进度条（纯长方体，不可拖拽）
    if (m_audioEngine && m_audioEngine->duration() > 0) {
        qint64 dur = m_audioEngine->duration();
        qint64 pos = qBound<qint64>(0, getGameTime(), dur);
        qreal progress = static_cast<qreal>(pos) / dur;
        qreal barY = 4;
        qreal barH = 4;
        qreal barW = w - 30;
        qreal barX = 15;

        // 背景
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 50, 200));
        painter.drawRoundedRect(QRectF(barX, barY, barW, barH), 2, 2);

        // 已播放部分
        painter.setBrush(QColor(0, 255, 136, 220));
        painter.drawRoundedRect(QRectF(barX, barY, barW * progress, barH), 2, 2);
    }

    QFont hudFont;
    hudFont.setPixelSize(20);
    painter.setFont(hudFont);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(15, 30, QStringLiteral("分数: %1").arg(
        m_scoreManager ? m_scoreManager->score() : 0));

    // Combo 大字动画
    int combo = m_scoreManager ? m_scoreManager->combo() : 0;
    if (combo > 0) {
        hudFont.setPixelSize(static_cast<int>(32 * m_comboScale));
        hudFont.setBold(true);
        painter.setFont(hudFont);

        // Combo 颜色：越高越炫
        QColor comboColor;
        if (combo < 10) comboColor = QColor(0, 255, 136);
        else if (combo < 30) comboColor = QColor(100, 200, 255);
        else if (combo < 50) comboColor = QColor(180, 120, 255);
        else if (combo < 100) comboColor = QColor(255, 180, 50);
        else comboColor = QColor(255, 80, 120);

        // Combo 光晕
        QRadialGradient comboGlow(w / 2.0, 75, 60 * m_comboScale);
        comboGlow.setColorAt(0.0, QColor(comboColor.red(), comboColor.green(), comboColor.blue(), 40));
        comboGlow.setColorAt(1.0, QColor(comboColor.red(), comboColor.green(), comboColor.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(comboGlow);
        painter.drawEllipse(QPointF(w / 2.0, 75), 80, 35);

        painter.setPen(comboColor);
        painter.drawText(QRectF(0, 50, w, 50), Qt::AlignCenter,
                         QStringLiteral("%1 Combo").arg(combo));
    }

    // 底部按键提示
    hudFont.setPixelSize(14);
    hudFont.setBold(false);
    painter.setFont(hudFont);
    painter.setPen(QColor(120, 120, 160));
    const QString keys4[] = {"D", "F", "J", "K"};
    const QString keys6[] = {"S", "D", "F", "J", "K", "L"};
    const QString* keys = (N == 4) ? keys4 : keys6;
    for (int i = 0; i < N; ++i) {
        qreal x = startX + i * m_trackWidth;
        painter.drawText(QRectF(x, h - 25, m_trackWidth, 20), Qt::AlignCenter, keys[i]);
    }

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
    if (m_laneCount == 4) {
        switch (event->key()) {
        case Qt::Key_D: lane = 0; break;
        case Qt::Key_F: lane = 1; break;
        case Qt::Key_J: lane = 2; break;
        case Qt::Key_K: lane = 3; break;
        }
    } else {
        switch (event->key()) {
        case Qt::Key_S: lane = 0; break;
        case Qt::Key_D: lane = 1; break;
        case Qt::Key_F: lane = 2; break;
        case Qt::Key_J: lane = 3; break;
        case Qt::Key_K: lane = 4; break;
        case Qt::Key_L: lane = 5; break;
        }
    }
    if (lane < 0) {
        switch (event->key()) {
        case Qt::Key_Escape:
            if (!m_gameActive) return;
            if (m_paused) { resumeGame(); m_pauseOverlay->hide(); }
            else { pauseGame(); m_pauseOverlay->setGeometry(0, 0, width(), height()); m_pauseOverlay->show(); m_continueBtn->setFocus(); }
            return;
        default: QWidget::keyPressEvent(event); return;
        }
    }

    m_keyPressed[lane] = true;
    if (m_gameActive && !m_paused) judgeLane(lane);
}

void GameWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    int lane = -1;
    if (m_laneCount == 4) {
        switch (event->key()) {
        case Qt::Key_D: lane = 0; break;
        case Qt::Key_F: lane = 1; break;
        case Qt::Key_J: lane = 2; break;
        case Qt::Key_K: lane = 3; break;
        }
    } else {
        switch (event->key()) {
        case Qt::Key_S: lane = 0; break;
        case Qt::Key_D: lane = 1; break;
        case Qt::Key_F: lane = 2; break;
        case Qt::Key_J: lane = 3; break;
        case Qt::Key_K: lane = 4; break;
        case Qt::Key_L: lane = 5; break;
        }
    }
    if (lane >= 0) {
        // Hold 尾部释放判定
        if (m_gameActive && !m_paused) {
            judgeHoldRelease(lane);
        }
        m_keyPressed[lane] = false;
    }
    else QWidget::keyReleaseEvent(event);
}

void GameWidget::onRenderTick()
{
    if (!m_gameActive || m_paused) return;

    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 deltaMs = (m_lastFrameTime > 0) ? (nowMs - m_lastFrameTime) : 16;
    m_lastFrameTime = nowMs;

    // 更新特效
    updateEffects(deltaMs);

    // 判定线脉冲衰减（已禁用）
    // m_judgeLinePulse *= 0.85;

    // Combo 弹跳回归
    m_comboScale += (1.0 - m_comboScale) * 0.15;

    // Combo 变化检测
    int combo = m_scoreManager ? m_scoreManager->combo() : 0;
    if (combo != m_lastCombo && combo > 0) {
        m_comboScale = 1.4;  // 弹跳
        m_lastCombo = combo;
    }

    // 检查超时未击打的音符
    checkMissedNotes();

    // Hold 持续粒子效果：每几帧为活跃 Hold 生成小粒子
    m_holdSparkleCounter++;
    if (m_holdSparkleCounter >= 3) {
        m_holdSparkleCounter = 0;
        qreal judgeY = height() * m_judgeLineY;
        for (auto it = m_activeHolds.begin(); it != m_activeHolds.end(); ++it) {
            int lane = it.key();
            qreal cx = laneX(lane) + m_trackWidth / 2.0;
            QColor color = laneColor(lane);

            // 生成 2 颗向上飘的小粒子
            for (int s = 0; s < 2; ++s) {
                HitParticle p;
                p.x = cx + (randFloat() - 0.5f) * m_trackWidth * 0.5;
                p.y = judgeY;
                p.vx = (randFloat() - 0.5f) * 1.5f;
                p.vy = -1.5f - randFloat() * 2.0f;  // 向上飘
                p.color = color.lighter(140 + static_cast<int>(randFloat() * 40));
                p.age = 0;
                p.maxAge = 15 + static_cast<int>(randFloat() * 10);
                p.size = 1.5f + randFloat() * 1.5f;
                m_particles.append(p);
            }
        }
    }

    // 判定文字动画倒计时
    if (m_judgeTextTimer > 0) {
        m_judgeTextTimer--;
    }

    // 检查游戏是否结束
    qint64 gameTime = getGameTime();
    if (m_audioEngine) {
        qint64 dur = m_audioEngine->duration();
        if (gameTime >= dur && dur > 0) {
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

        if (note.noteType == HOLD && note.holdActive) {
            // ── 活跃 Hold：检查是否需要自动完成 ──
            // 尾部时间已过且玩家仍按住 → 自动完成（晚松不扣分）
            if (currentPos >= note.holdEndMs()) {
                note.holdActive = false;
                note.holdFinished = true;
                note.judged = true;

                // 此时才加分
                if (m_scoreManager && note.judgment != 3) {
                    m_scoreManager->scoreHoldComplete(note.judgment);
                }

                // Hold 完成特效
                spawnHoldCompleteEffect(note.lane, note.judgment);

                // 清理活跃 Hold 记录
                m_activeHolds.remove(note.lane);
            }
            // 注意：早松情况由 keyReleaseEvent 处理，此处不重复
            continue;
        }

        // Tap 音符 或 Hold 头部未按：超过判定线 + MISS_THRESHOLD → Miss
        if (currentPos - note.timestampMs > MISS_THRESHOLD) {
            note.judged = true;
            note.judgment = 3; // Miss

            if (m_scoreManager) {
                m_scoreManager->addMiss();
            }

            // 如果是 Hold 音符头部 Miss，清理活跃记录
            if (note.noteType == HOLD) {
                m_activeHolds.remove(note.lane);
            }
        }
    }
}

void GameWidget::judgeLane(int lane)
{
    if (!m_scoreManager) return;

    qint64 hitTime = getGameTime();

    // 找到该轨道"最近且在判定窗口内"的未判定音符
    // 判定窗口 = GOOD_WINDOW(150ms) + MISS_THRESHOLD(300ms) = 450ms
    // 超过这个窗口的音符不允许提前按键判定
    static const qint64 JUDGE_WINDOW = 450;  // ±450ms 内才允许判定
    qint64 minDelta = INT64_MAX;
    int bestIdx = -1;

    for (int i = 0; i < m_notes.size(); ++i) {
        GameNote& note = m_notes[i];
        if (note.judged || note.lane != lane) continue;
        if (note.holdActive) continue;  // 跳过正在按住的 Hold

        qint64 delta = qAbs(hitTime - note.timestampMs);
        if (delta > JUDGE_WINDOW) continue;  // 超出判定窗口，不允许提前判定

        if (delta < minDelta) {
            minDelta = delta;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) return;  // 没有可判定的音符（空按无效）

    GameNote& note = m_notes[bestIdx];

    // 判定头部
    int result;
    if (note.noteType == HOLD) {
        // Hold 头部：只检查不加分，等长条完成后再计分
        result = m_scoreManager->checkHit(hitTime, note.timestampMs);
        note.judgment = result;

        if (result != 3) {
            // Hold 头部判定成功 → 进入持续按住状态（不加分）
            note.holdActive = true;
            note.judged = false;  // 整体未判定完成，等尾端
            m_activeHolds[lane] = bestIdx;
            // Hold 头部特效（增强爆发）
            spawnHoldHeadEffect(lane, result);
        } else {
            // Hold 头部 Miss → 直接失败
            note.judged = true;
            if (m_scoreManager) {
                m_scoreManager->addMiss();
            }
            spawnHitEffect(lane, 3);
        }
    } else {
        // Tap 音符：立即判定加分
        result = m_scoreManager->judgeHit(hitTime, note.timestampMs);
        note.judgment = result;
        note.judged = true;
        // Tap 特效
        spawnHitEffect(lane, result);
    }

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

void GameWidget::judgeHoldRelease(int lane)
{
    // 检查该轨道是否有活跃的 Hold
    if (!m_activeHolds.contains(lane)) return;

    int idx = m_activeHolds[lane];
    if (idx < 0 || idx >= m_notes.size()) {
        m_activeHolds.remove(lane);
        return;
    }

    GameNote& note = m_notes[idx];
    if (!note.holdActive) {
        m_activeHolds.remove(lane);
        return;
    }

    qint64 currentPos = getGameTime();
    qint64 tailTime = note.holdEndMs();

    // 释放判定
    if (currentPos >= tailTime) {
        // 在尾部时间或之后释放 → 成功（晚松不扣分）
        // 此时加分
        note.holdActive = false;
        note.holdFinished = true;
        note.judged = true;

        if (m_scoreManager && note.judgment != 3) {
            m_scoreManager->scoreHoldComplete(note.judgment);
        }

        // Hold 完成特效
        spawnHoldCompleteEffect(lane, note.judgment);
    } else {
        // 在尾部时间之前释放 → 早松 = Miss
        note.holdActive = false;
        note.judged = true;
        note.judgment = 3; // Miss

        if (m_scoreManager) {
            m_scoreManager->addMiss();
        }

        // 显示 Miss 文字
        m_judgeText = QStringLiteral("Miss");
        m_judgeTextColor = QColor(233, 69, 96);
        m_judgeTextLane = lane;
        m_judgeTextTimer = 10;

        spawnHitEffect(lane, 3);
    }

    m_activeHolds.remove(lane);
}

QColor GameWidget::laneColor(int lane) const
{
    switch (lane) {
    case 0: return m_laneCount == 4 ? QColor(0, 255, 136)   // 4键 D: 荧光绿
                                    : QColor(0, 240, 255);  // 6键 S: 青色
    case 1: return QColor(0, 255, 136);   // D: 荧光绿
    case 2: return QColor(189, 147, 249); // F: 浅紫
    case 3: return QColor(139, 233, 253); // J: 浅蓝
    case 4: return QColor(255, 121, 198); // K: 粉色
    case 5: return QColor(255, 180, 50);  // L: 金色
    default: return QColor(255, 255, 255);
    }
}

qreal GameWidget::laneX(int lane) const
{
    qreal totalTrackWidth = m_trackWidth * m_laneCount;
    qreal startX = (width() - totalTrackWidth) / 2.0;
    return startX + lane * m_trackWidth;
}

void GameWidget::spawnHitEffect(int lane, int judgment)
{
    qreal cx = laneX(lane) + m_trackWidth / 2.0;
    qreal cy = height() * m_judgeLineY;
    QColor color = laneColor(lane);

    // 判定线脉冲（已禁用）
    // m_judgeLinePulse = 1.0;

    // 命中反馈环（2 层，颜色不同）
    QColor ringColor = (judgment == 1) ? QColor(0, 255, 136) :
                       (judgment == 2) ? QColor(255, 220, 50) : QColor(233, 69, 96);
    m_rings.append({cx, cy, ringColor, 0, 20, 5.0, 45.0});
    m_rings.append({cx, cy, color, 0, 16, 0.0, 30.0});

    // 粒子爆发（8-12 颗）
    int particleCount = (judgment == 1) ? 12 : (judgment == 2) ? 8 : 4;
    for (int i = 0; i < particleCount; ++i) {
        float angle = randFloat() * 6.28318f;
        float speed = 1.5f + randFloat() * 3.0f;
        HitParticle p;
        p.x = cx;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed - 1.0f;  // 略微向上偏
        p.color = (judgment == 1) ? color.lighter(130) :
                  (judgment == 2) ? QColor(255, 220, 100) : QColor(200, 80, 100);
        p.age = 0;
        p.maxAge = 20 + static_cast<int>(randFloat() * 15);
        p.size = 2.0f + randFloat() * 2.0f;
        m_particles.append(p);
    }

    // 分数弹出
    QString popupText;
    QColor popupColor;
    if (judgment == 1) {
        popupText = QStringLiteral("+100");
        popupColor = QColor(0, 255, 136);
    } else if (judgment == 2) {
        popupText = QStringLiteral("+50");
        popupColor = QColor(255, 220, 50);
    } else {
        popupText = QStringLiteral("Miss");
        popupColor = QColor(233, 69, 96);
    }
    m_popups.append({popupText, popupColor, cx, cy - 30, 0, 30});
}

void GameWidget::spawnHoldHeadEffect(int lane, int judgment)
{
    qreal cx = laneX(lane) + m_trackWidth / 2.0;
    qreal cy = height() * m_judgeLineY;
    QColor color = laneColor(lane);

    // 判定结果颜色
    QColor judgeColor = (judgment == 1) ? QColor(0, 255, 136) :
                        (judgment == 2) ? QColor(255, 220, 50) : QColor(233, 69, 96);

    // 双层反馈环（比 Tap 更大）
    m_rings.append({cx, cy, judgeColor, 0, 25, 8.0, 60.0});
    m_rings.append({cx, cy, color, 0, 20, 0.0, 40.0});
    m_rings.append({cx, cy, QColor(255, 255, 255), 0, 12, 0.0, 20.0});

    // 密集粒子爆发（16 颗，比 Tap 多）
    int particleCount = (judgment == 1) ? 16 : (judgment == 2) ? 12 : 6;
    for (int i = 0; i < particleCount; ++i) {
        float angle = randFloat() * 6.28318f;
        float speed = 2.0f + randFloat() * 4.0f;
        HitParticle p;
        p.x = cx;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed - 1.5f;
        p.color = (judgment == 1) ? color.lighter(150) :
                  (judgment == 2) ? QColor(255, 220, 100) : QColor(200, 80, 100);
        p.age = 0;
        p.maxAge = 25 + static_cast<int>(randFloat() * 20);
        p.size = 2.0f + randFloat() * 3.0f;
        m_particles.append(p);
    }

    // 分数弹出（Hold 头部不加分，显示 HOLD 提示）
    m_popups.append({QStringLiteral("HOLD!"), judgeColor, cx, cy - 35, 0, 35});
}

void GameWidget::spawnHoldCompleteEffect(int lane, int judgment)
{
    qreal cx = laneX(lane) + m_trackWidth / 2.0;
    qreal cy = height() * m_judgeLineY;
    QColor color = laneColor(lane);

    if (judgment == 3) return;  // Miss 不需要完成特效

    QColor judgeColor = (judgment == 1) ? QColor(0, 255, 136) : QColor(255, 220, 50);

    // 完成时的大环
    m_rings.append({cx, cy, judgeColor, 0, 30, 10.0, 70.0});
    m_rings.append({cx, cy, QColor(255, 255, 255, 200), 0, 18, 0.0, 35.0});

    // 向上飞溅的粒子（庆祝感）
    int particleCount = 14;
    for (int i = 0; i < particleCount; ++i) {
        float angle = -3.14159f / 2.0f + (randFloat() - 0.5f) * 2.0f;  // 向上扇形
        float speed = 2.0f + randFloat() * 3.5f;
        HitParticle p;
        p.x = cx + (randFloat() - 0.5f) * m_trackWidth * 0.6;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.color = color.lighter(140 + static_cast<int>(randFloat() * 60));
        p.age = 0;
        p.maxAge = 30 + static_cast<int>(randFloat() * 20);
        p.size = 2.5f + randFloat() * 2.5f;
        m_particles.append(p);
    }

    // 分数弹出
    int scoreVal = (judgment == 1) ? 300 : 100;
    m_popups.append({QStringLiteral("+%1").arg(scoreVal), judgeColor, cx, cy - 40, 0, 35});
}

void GameWidget::updateEffects(qint64 deltaTimeMs)
{
    float dt = deltaTimeMs / 16.0f;  // 归一化到 ~60fps

    // 更新粒子
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        HitParticle& p = m_particles[i];
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 0.15f * dt;  // 重力
        p.vx *= 0.97f;
        p.age += static_cast<int>(dt);
        if (p.age >= p.maxAge) {
            m_particles.removeAt(i);
        }
    }

    // 更新反馈环
    for (int i = m_rings.size() - 1; i >= 0; --i) {
        m_rings[i].age += static_cast<int>(dt);
        if (m_rings[i].age >= m_rings[i].maxAge) {
            m_rings.removeAt(i);
        }
    }

    // 更新分数弹出
    for (int i = m_popups.size() - 1; i >= 0; --i) {
        m_popups[i].age += static_cast<int>(dt);
        if (m_popups[i].age >= m_popups[i].maxAge) {
            m_popups.removeAt(i);
        }
    }
}
