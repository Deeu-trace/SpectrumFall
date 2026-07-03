#include "ChartTimelineWidget.h"
#include "AudioEngine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QPolygon>
#include <QPolygonF>
#include <QMediaPlayer>
#include <QTimer>
#include <algorithm>

// 轨道颜色（与 GameWidget::laneColor() 6K 模式一致）
const QColor ChartTimelineWidget::LANE_COLORS[LANE_COUNT] = {
    QColor(  0, 240, 255),   // S: 青
    QColor(  0, 255, 136),   // D: 绿
    QColor(189, 147, 249),   // F: 紫
    QColor(139, 233, 253),   // J: 浅蓝
    QColor(255, 121, 198),   // K: 粉
    QColor(255, 180,  50),   // L: 金
};

ChartTimelineWidget::ChartTimelineWidget(QWidget* parent)
    : QWidget(parent)
    , m_audio(nullptr)
    , m_selectedIndex(-1)
    , m_durationMs(0)
    , m_positionMs(0)
    , m_waveformValid(false)
    , m_flashLane(-1)
    , m_flashAlpha(0)
    , m_flashTimer(nullptr)
    , m_zoomFactor(1.0f)
    , m_zoomDebounce(nullptr)
    , m_draggingPlayhead(false)
    , m_wasPlayingBeforeDrag(false)
    , m_draggingToCreate(false)
    , m_dragStartX(0)
    , m_dragStartTimeMs(0)
    , m_dragLane(-1)
    , m_dragCurrentX(0)
{
    setMinimumHeight(200);
    setMouseTracking(true);  // 需要追踪鼠标以改变播放头附近的游标
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void ChartTimelineWidget::setAudioEngine(AudioEngine* audio)
{
    m_audio = audio;
    m_waveformValid = false;
    update();
}

void ChartTimelineWidget::setNotes(const QVector<GameNote>& notes)
{
    m_notes = notes;
    m_selectedIndex = -1;  // 音符列表变化时清除选中
    update();
}

void ChartTimelineWidget::setSelectedIndex(int index)
{
    if (index < -1 || index >= m_notes.size()) index = -1;
    if (m_selectedIndex == index) return;
    m_selectedIndex = index;
    update();
}

void ChartTimelineWidget::setLaneCount(int count)
{
    if (count < 1 || count > LANE_COUNT) return;
    m_laneCount = count;
    update();
}

void ChartTimelineWidget::setDurationMs(qint64 ms)
{
    m_durationMs = ms;
    m_waveformValid = false;
    update();
}

void ChartTimelineWidget::setPositionMs(qint64 ms)
{
    qint64 oldPos = m_positionMs;
    m_positionMs = ms;
    // 局部重绘：刷新旧播放头 + 新播放头所在的窄条
    if (m_durationMs > 0) {
        int stripW = PLAYHEAD_GRAB_RADIUS * 2 + 6;
        if (oldPos > 0) {
            int oldX = timeToX(oldPos);
            update(QRect(oldX - PLAYHEAD_GRAB_RADIUS - 2, 0, stripW, height()));
        }
        if (ms > 0) {
            int newX = timeToX(ms);
            update(QRect(newX - PLAYHEAD_GRAB_RADIUS - 2, 0, stripW, height()));
        }
    }
}

void ChartTimelineWidget::flashLane(int lane)
{
    if (lane < 0 || lane >= m_laneCount) return;
    m_flashLane  = lane;
    m_flashAlpha = 120;

    if (!m_flashTimer) {
        m_flashTimer = new QTimer(this);
        m_flashTimer->setInterval(30);  // ~33 fps 衰减
        connect(m_flashTimer, &QTimer::timeout, this, [this]() {
            m_flashAlpha -= 15;
            if (m_flashAlpha <= 0) {
                m_flashAlpha = 0;
                m_flashLane  = -1;
                m_flashTimer->stop();
            }
            update();
        });
    }
    m_flashTimer->start();
    update();
}

// ── 缩放 ──────────────────────────────────────────────────────

QSize ChartTimelineWidget::sizeHint() const
{
    return QSize(static_cast<int>(BASE_WIDTH * m_zoomFactor), 200);
}

void ChartTimelineWidget::setZoomFactor(float factor)
{
    factor = qBound(1.0f, factor, 10.0f);
    if (qFuzzyCompare(factor, m_zoomFactor)) return;
    m_zoomFactor = factor;

    // 延迟 50ms 再应用几何变化 + 重建波形，避免拖拽滑块时每帧都重建
    if (!m_zoomDebounce) {
        m_zoomDebounce = new QTimer(this);
        m_zoomDebounce->setSingleShot(true);
        m_zoomDebounce->setInterval(50);
        connect(m_zoomDebounce, &QTimer::timeout, this, &ChartTimelineWidget::applyZoomGeometry);
    }
    m_zoomDebounce->start();   // 重置 50ms 倒计时
    update();                  // 立即重绘（用旧波形），保持视觉流畅
}

void ChartTimelineWidget::applyZoomGeometry()
{
    int newW = static_cast<int>(BASE_WIDTH * m_zoomFactor);
    int curH = height() > 100 ? height() : 200;
    // 强制设定宽度 —— 不依赖 sizeHint / updateGeometry
    setMinimumWidth(newW);
    resize(newW, curH);
    m_waveformValid = false;   // 宽度变化需重建波形
    update();
}

void ChartTimelineWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        float delta = event->angleDelta().y() / 120.0f;
        float newZoom = m_zoomFactor + delta * 0.5f;
        setZoomFactor(newZoom);
        emit zoomChanged(m_zoomFactor);
        event->accept();
        return;
    }
    // 普通滚轮：水平滚动时间轴（上滚=左移/更早，下滚=右移/更晚）
    int delta = event->angleDelta().y();
    if (delta != 0) {
        emit scrollRequested(-delta);
    }
    event->accept();
}

// ── 布局计算 ──────────────────────────────────────────────────

int ChartTimelineWidget::laneAreaTop() const
{
    return WAVEFORM_HEIGHT + 2;  // 波形下方留 2px 间隙
}

int ChartTimelineWidget::laneAreaBottom() const
{
    return height() - 2;  // 底部留 2px 边距
}

int ChartTimelineWidget::laneHeight() const
{
    int area = laneAreaBottom() - laneAreaTop();
    return area / m_laneCount;
}

int ChartTimelineWidget::timeToX(qint64 ms) const
{
    if (m_durationMs <= 0) return 0;
    return static_cast<int>((static_cast<qint64>(width()) * ms) / m_durationMs);
}

qint64 ChartTimelineWidget::xToTime(int x) const
{
    if (width() <= 0) return 0;
    return static_cast<qint64>(x) * m_durationMs / width();
}

int ChartTimelineWidget::laneToY(int lane) const
{
    return laneAreaTop() + lane * laneHeight() + laneHeight() / 2;
}

int ChartTimelineWidget::yToLane(int y) const
{
    int lh = laneHeight();
    if (lh <= 0) return 0;
    int lane = (y - laneAreaTop()) / lh;
    if (lane < 0) lane = 0;
    if (lane >= m_laneCount) lane = m_laneCount - 1;
    return lane;
}

// ── 波形构建 ──────────────────────────────────────────────────

void ChartTimelineWidget::rebuildWaveform()
{
    m_waveform.clear();
    m_waveformValid = true;  // 标记为已构建（即使数据为空也不再重试）

    if (!m_audio || !m_audio->isLoaded() || m_durationMs <= 0 || width() <= 0)
        return;

    const QVector<float>& pcm = m_audio->pcmData();
    if (pcm.isEmpty()) return;

    int w = width();
    int bins = w;  // 1 像素 = 1 bin（缩放后宽度变化自动适配）
    if (bins <= 0) return;

    m_waveform.resize(bins);

    qint64 totalSamples = pcm.size();
    qint64 samplesPerBin = totalSamples / bins;
    if (samplesPerBin < 1) samplesPerBin = 1;

    for (int i = 0; i < bins; ++i) {
        qint64 start = i * samplesPerBin;
        qint64 end = start + samplesPerBin;
        if (end > totalSamples) end = totalSamples;

        float peak = 0.0f;
        for (qint64 j = start; j < end; ++j) {
            float v = std::abs(pcm[j]);
            if (v > peak) peak = v;
        }
        m_waveform[i].peak = peak;
    }
}

// ── 绘制 ──────────────────────────────────────────────────────

void ChartTimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // 关闭抗锯齿：直线和矩形不需要 AA，显著提升绘制速度
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // 背景
    p.fillRect(0, 0, w, h, QColor(13, 13, 31));

    // ── 波形 ──
    if (!m_waveformValid) {
        rebuildWaveform();
    }

    if (!m_waveform.isEmpty()) {
        int wfH = WAVEFORM_HEIGHT;
        int centerY = wfH / 2;
        int halfH = centerY - 2;

        int bins = m_waveform.size();
        float binW = static_cast<float>(w) / bins;

        // 用多边形一次性填充波形，比逐像素 drawLine 快数十倍
        QPolygonF wavePoly;
        wavePoly.reserve(bins * 2 + 2);
        // 上半部分（从左到右）
        for (int i = 0; i < bins; ++i) {
            float peak = m_waveform[i].peak;
            int barH = static_cast<int>(peak * halfH);
            if (barH < 1) barH = 1;
            float x = i * binW;
            wavePoly.append(QPointF(x, centerY - barH));
        }
        // 下半部分（从右到左）
        for (int i = bins - 1; i >= 0; --i) {
            float peak = m_waveform[i].peak;
            int barH = static_cast<int>(peak * halfH);
            if (barH < 1) barH = 1;
            float x = (i + 1) * binW;
            wavePoly.append(QPointF(x, centerY + barH));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 255, 136, 140));
        p.drawPolygon(wavePoly);
    } else {
        // 无波形数据时显示提示
        p.setPen(QColor(100, 100, 120));
        p.drawText(QRect(0, 0, w, WAVEFORM_HEIGHT), Qt::AlignCenter,
                   QStringLiteral("无波形数据"));
    }

    // 波形与轨道的分隔线
    p.setPen(QPen(QColor(30, 30, 60), 1));
    p.drawLine(0, WAVEFORM_HEIGHT, w, WAVEFORM_HEIGHT);

    // ── 轨道色带 ──
    int laTop = laneAreaTop();
    int lh = laneHeight();

    for (int lane = 0; lane < m_laneCount; ++lane) {
        int y0 = laTop + lane * lh;

        // 交替背景
        QColor bg = (lane % 2 == 0) ? QColor(18, 18, 38) : QColor(22, 22, 48);
        p.fillRect(0, y0, w, lh, bg);

        // 左侧色条标识
        p.fillRect(0, y0 + 1, 3, lh - 2, LANE_COLORS[lane]);

        // 轨道分隔线
        if (lane > 0) {
            p.setPen(QPen(QColor(30, 30, 60, 120), 1));
            p.drawLine(0, y0, w, y0);
        }
    }

    // ── 轨道闪烁（实时录入反馈）──
    if (m_flashLane >= 0 && m_flashLane < m_laneCount && m_flashAlpha > 0) {
        int y0 = laTop + m_flashLane * lh;
        QColor flashColor = LANE_COLORS[m_flashLane];
        flashColor.setAlpha(m_flashAlpha);
        p.fillRect(0, y0, w, lh, flashColor);
    }

    // ── 音符标记 ──
    for (int i = 0; i < m_notes.size(); ++i) {
        const GameNote& note = m_notes[i];
        if (note.lane < 0 || note.lane >= m_laneCount) continue;  // 防御性跳过
        int x = timeToX(note.timestampMs);
        int y = laneToY(note.lane);
        if (x < 0 || x > w) continue;

        const QColor& color = LANE_COLORS[note.lane];
        const bool selected = (i == m_selectedIndex);

        if (note.noteType == HOLD && note.holdDurationMs > 0) {
            // HOLD: 横向条
            int xEnd = timeToX(note.timestampMs + note.holdDurationMs);
            int barH = lh - 6;
            QRect bar(x, y - barH / 2, xEnd - x, barH);
            p.setPen(Qt::NoPen);
            p.setBrush(color.lighter(120));
            p.drawRoundedRect(bar, 3, 3);
            // 头部圆点
            p.setBrush(color);
            p.drawEllipse(QPoint(x, y), 4, 4);
            // 选中白边
            if (selected) {
                p.setPen(QPen(Qt::white, 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(bar.adjusted(-1, -1, 1, 1), 4, 4);
            }
        } else {
            // TAP: 圆点
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPoint(x, y), 5, 5);
            // 外圈
            if (selected) {
                p.setPen(QPen(Qt::white, 2));
            } else {
                p.setPen(QPen(color.darker(150), 1));
            }
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPoint(x, y), selected ? 7 : 5, selected ? 7 : 5);
        }
    }

    // ── 拖拽创建 HOLD 预览 ──
    if (m_draggingToCreate && m_dragLane >= 0 && m_dragLane < m_laneCount) {
        int x1 = qMin(m_dragStartX, m_dragCurrentX);
        int x2 = qMax(m_dragStartX, m_dragCurrentX);
        int y = laneToY(m_dragLane);
        int barH = lh - 6;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 255, 136, 80));
        p.drawRoundedRect(QRect(x1, y - barH / 2, x2 - x1, barH), 3, 3);
        // 边框

        p.setPen(QPen(QColor(0, 255, 136, 160), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRect(x1, y - barH / 2, x2 - x1, barH), 3, 3);
    }

    // ── 播放头 ──
    if (m_positionMs > 0 && m_durationMs > 0) {
        int px = timeToX(m_positionMs);
        if (px >= 0 && px <= w) {
            p.setPen(QPen(QColor(255, 255, 255, 200), 2));
            p.drawLine(px, 0, px, h);
            // 顶部三角标记
            p.setBrush(QColor(255, 255, 255, 220));
            p.setPen(Qt::NoPen);
            QPolygon tri;
            tri << QPoint(px - 5, 0) << QPoint(px + 5, 0) << QPoint(px, 7);
            p.drawPolygon(tri);
        }
    }

    // 外框
    p.setPen(QPen(QColor(15, 52, 96), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, 0, w - 1, h - 1);
}

void ChartTimelineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_waveformValid = false;  // 尺寸变化需重建波形
}

// ── 鼠标交互 ──────────────────────────────────────────────────

bool ChartTimelineWidget::isNearPlayhead(int x) const
{
    if (m_positionMs <= 0 || m_durationMs <= 0) return false;
    int px = timeToX(m_positionMs);
    return std::abs(x - px) <= PLAYHEAD_GRAB_RADIUS;
}

void ChartTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_durationMs <= 0) return;

    int x = event->pos().x();
    int y = event->pos().y();

    // 右键：删除音符
    if (event->button() == Qt::RightButton) {
        int hitIdx = findNoteAt(x, y);
        if (hitIdx >= 0) {
            emit noteDeleted(hitIdx);
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    // 检查是否点击了播放头（优先于其他操作）
    if (isNearPlayhead(x)) {
        m_draggingPlayhead = true;
        // 拖拽前暂停音频，防止 onPositionUpdate 覆盖拖拽位置（消除抽搐）
        if (m_audio && m_audio->isLoaded()) {
            m_wasPlayingBeforeDrag = (m_audio->state() == QMediaPlayer::PlayingState);
            if (m_wasPlayingBeforeDrag) {
                m_audio->pause();
            }
        }
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // 波形区域：点击跳转播放位置
    if (y < WAVEFORM_HEIGHT) {
        qint64 t = xToTime(x);
        if (m_audio && m_audio->isLoaded()) {
            m_audio->seek(t);
            m_positionMs = t;
            update();
        }
        return;
    }

    // 轨道区域：先检查是否点击了已有音符
    int hitIdx = findNoteAt(x, y);
    if (hitIdx >= 0) {
        emit noteSelected(hitIdx);
        return;
    }

    // 空白处：记录拖拽起点（松开时区分 click vs drag）
    m_draggingToCreate = true;
    m_dragStartX = x;
    m_dragStartTimeMs = xToTime(x);
    m_dragLane = yToLane(y);
    m_dragCurrentX = x;
}

void ChartTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    int x = event->pos().x();

    // 正在拖拽播放头
    if (m_draggingPlayhead) {
        qint64 oldMs = m_positionMs;
        qint64 t = qBound(qint64(0), xToTime(x), m_durationMs);
        m_positionMs = t;
        emit playheadDragged(t);
        // 局部重绘：刷新旧 + 新播放头窄条
        if (m_durationMs > 0) {
            int stripW = PLAYHEAD_GRAB_RADIUS * 2 + 6;
            if (oldMs > 0) {
                int oldX = timeToX(oldMs);
                update(QRect(oldX - PLAYHEAD_GRAB_RADIUS - 2, 0, stripW, height()));
            }
            if (t > 0) {
                int newX = timeToX(t);
                update(QRect(newX - PLAYHEAD_GRAB_RADIUS - 2, 0, stripW, height()));
            }
        }
        return;
    }

    // 正在拖拽创建 HOLD
    if (m_draggingToCreate) {
        int oldMinX = qMin(m_dragStartX, m_dragCurrentX);
        int oldMaxX = qMax(m_dragStartX, m_dragCurrentX);
        m_dragCurrentX = x;
        int newMinX = qMin(m_dragStartX, x);
        int newMaxX = qMax(m_dragStartX, x);
        // 刷新旧预览区域 + 新预览区域
        int unionMinX = qMin(oldMinX, newMinX);
        int unionMaxX = qMax(oldMaxX, newMaxX);
        update(QRect(unionMinX - 2, 0, unionMaxX - unionMinX + 4, height()));
        return;
    }

    // 非拖拽状态：根据鼠标位置改变游标
    if (m_durationMs > 0 && isNearPlayhead(x)) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void ChartTimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggingPlayhead && event->button() == Qt::LeftButton) {
        m_draggingPlayhead = false;
        setCursor(Qt::ArrowCursor);

        // seek 到最终位置
        qint64 t = qBound(qint64(0), xToTime(event->pos().x()), m_durationMs);
        if (m_audio && m_audio->isLoaded()) {
            m_audio->seek(t);
            // 如果拖拽前在播放，恢复播放
            if (m_wasPlayingBeforeDrag) {
                m_audio->play();
            }
        }
        m_positionMs = t;
        m_wasPlayingBeforeDrag = false;
        update();  // 释放时全量刷新一次（确保播放头在新位置正确绘制）
    }

    // 拖拽创建 HOLD 释放
    if (m_draggingToCreate && event->button() == Qt::LeftButton) {
        m_draggingToCreate = false;
        int endX = event->pos().x();
        int dragDist = std::abs(endX - m_dragStartX);

        if (dragDist > MIN_DRAG_THRESHOLD) {
            // 拖拽距离足够 → 创建 HOLD 音符
            qint64 endTimeMs = xToTime(endX);
            qint64 startMs = qMin(m_dragStartTimeMs, endTimeMs);
            qint64 durationMs = qAbs(endTimeMs - m_dragStartTimeMs);
            if (durationMs > 0) {
                emit holdNoteAdded(startMs, durationMs, m_dragLane);
            }
        } else {
            // 短点击 → 创建 TAP 音符
            emit noteAdded(m_dragStartTimeMs, m_dragLane);
        }
        update();  // 清除预览
    }
}

int ChartTimelineWidget::findNoteAt(int x, int y) const
{
    for (int i = 0; i < m_notes.size(); ++i) {
        const GameNote& note = m_notes[i];
        if (note.lane < 0 || note.lane >= m_laneCount) continue;
        int nx = timeToX(note.timestampMs);
        int ny = laneToY(note.lane);

        // HOLD 音符的命中区域扩展到整个条
        if (note.noteType == HOLD && note.holdDurationMs > 0) {
            int xEnd = timeToX(note.timestampMs + note.holdDurationMs);
            if (x >= nx - NOTE_HIT_RADIUS && x <= xEnd + NOTE_HIT_RADIUS &&
                std::abs(y - ny) <= NOTE_HIT_RADIUS + 2) {
                return i;
            }
        } else {
            int dx = x - nx;
            int dy = y - ny;
            if (dx * dx + dy * dy <= NOTE_HIT_RADIUS * NOTE_HIT_RADIUS) {
                return i;
            }
        }
    }
    return -1;
}
