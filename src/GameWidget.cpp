#include "GameWidget.h"
#include "AudioEngine.h"
#include "ScoreManager.h"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QApplication>
#include <cmath>

// 判定线和轨道参数常量
static constexpr qreal SCROLL_SPEED = 0.35;     // 像素/毫秒（下落速度，降低让玩家有更多反应时间）
static constexpr qint64 VISIBLE_AHEAD = 3000;   // 提前 3 秒显示音符
static constexpr qint64 MISS_THRESHOLD = 300;   // 超过 300ms 视为 Miss（音符过了判定线后的容许窗口）
static constexpr int LANE_COUNT = 4;

GameWidget::GameWidget(AudioEngine* audioEngine, ScoreManager* scoreManager, QWidget* parent)
    : QWidget(parent)
    , m_audioEngine(audioEngine)
    , m_scoreManager(scoreManager)
    , m_renderTimer(new QTimer(this))
    , m_paused(false)
    , m_gameActive(false)
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
}

void GameWidget::startGame(const QVector<GameNote>& notes)
{
    m_notes = notes;
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

    // 暂停提示
    if (m_paused) {
        painter.fillRect(rect(), QColor(0, 0, 0, 150));

        QFont pauseFont;
        pauseFont.setPixelSize(40);
        pauseFont.setBold(true);
        painter.setFont(pauseFont);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂停\n按 Esc 继续"));
    }

    // 底部按键提示
    hudFont.setPixelSize(14);
    hudFont.setBold(false);
    painter.setFont(hudFont);
    painter.setPen(QColor(120, 120, 160));
    const QString keys[] = {"D", "F", "J", "K"};
    for (int i = 0; i < LANE_COUNT; ++i) {
        qreal x = startX + i * m_trackWidth;
        painter.drawText(QRectF(x, h - 25, m_trackWidth, 20), Qt::AlignCenter, keys[i]);
    }
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
        if (m_paused) {
            resumeGame();
        } else if (m_gameActive) {
            pauseGame();
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
