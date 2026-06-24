#include "ChartTimelineWidget.h"
#include "AudioEngine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPolygon>
#include <algorithm>

// 轨道颜色（红橙黄绿蓝紫）
const QColor ChartTimelineWidget::LANE_COLORS[LANE_COUNT] = {
    QColor(255,  85,  85),
    QColor(255, 153,  85),
    QColor(255, 221,  85),
    QColor( 85, 255, 119),
    QColor( 85, 170, 255),
    QColor(170, 119, 255),
};

ChartTimelineWidget::ChartTimelineWidget(QWidget* parent)
    : QWidget(parent)
    , m_audio(nullptr)
    , m_durationMs(0)
    , m_positionMs(0)
    , m_waveformValid(false)
{
    setMinimumHeight(200);
    setMouseTracking(false);
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
    m_positionMs = ms;
    update();
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
    return area / LANE_COUNT;
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
    if (lane >= LANE_COUNT) lane = LANE_COUNT - 1;
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
    int bins = w * 2;  // 2 倍过采样，视觉更细腻
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
    p.setRenderHint(QPainter::Antialiasing, true);

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

        p.setPen(QPen(QColor(0, 255, 136, 180), 1));
        for (int i = 0; i < bins; ++i) {
            float peak = m_waveform[i].peak;
            int barH = static_cast<int>(peak * halfH);
            if (barH < 1) barH = 1;
            int x = static_cast<int>(i * binW);
            int x2 = static_cast<int>((i + 1) * binW);
            p.drawLine(x, centerY - barH, x, centerY + barH);
            // 填充间隙
            if (x2 > x + 1) {
                p.drawLine(x + 1, centerY - barH, x2 - 1, centerY + barH);
            }
        }
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

    for (int lane = 0; lane < LANE_COUNT; ++lane) {
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

    // ── 音符标记 ──
    for (int i = 0; i < m_notes.size(); ++i) {
        const GameNote& note = m_notes[i];
        if (note.lane < 0 || note.lane >= LANE_COUNT) continue;  // 防御性跳过
        int x = timeToX(note.timestampMs);
        int y = laneToY(note.lane);
        if (x < 0 || x > w) continue;

        const QColor& color = LANE_COLORS[note.lane];

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
        } else {
            // TAP: 圆点
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPoint(x, y), 5, 5);
            // 外圈
            p.setPen(QPen(color.darker(150), 1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPoint(x, y), 5, 5);
        }
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

void ChartTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (m_durationMs <= 0) return;

    int x = event->pos().x();
    int y = event->pos().y();

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

    // 空白处：添加新音符
    int lane = yToLane(y);
    qint64 timeMs = xToTime(x);
    emit noteAdded(timeMs, lane);
}

int ChartTimelineWidget::findNoteAt(int x, int y) const
{
    for (int i = 0; i < m_notes.size(); ++i) {
        const GameNote& note = m_notes[i];
        if (note.lane < 0 || note.lane >= LANE_COUNT) continue;
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
