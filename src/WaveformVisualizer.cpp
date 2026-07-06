#include "WaveformVisualizer.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

WaveformVisualizer::WaveformVisualizer(QWidget* parent)
    : VisualizerBase(parent)
{
}

void WaveformVisualizer::setWaveformData(const QVector<float>& samples)
{
    m_waveform = samples;
    update();
}

void WaveformVisualizer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    ThemePalette p = ThemeManager::instance()->gamePalette();

    // 背景
    painter.fillRect(rect(), QColor(p.visBg));

    if (m_waveform.isEmpty()) {
        return;
    }

    int w = width();
    int h = height();
    int midY = h / 2;

    // 辉光效果：先画宽线（模糊层），再画细线（清晰层）
    QColor accent(p.primary);
    QVector<QPointF> points;
    int step = qMax(1, m_waveform.size() / w);

    for (int x = 0; x < w; ++x) {
        int idx = x * step;
        if (idx < m_waveform.size()) {
            float val = m_waveform[idx];
            int y = midY - static_cast<int>(val * midY * 0.8f);
            points.append(QPointF(x, y));
        }
    }

    if (points.size() < 2) {
        return;
    }

    // 辉光层（宽线，半透明）
    QPen glowPen(QColor(accent.red(), accent.green(), accent.blue(), 60), 6);
    glowPen.setCapStyle(Qt::RoundCap);
    glowPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(glowPen);
    painter.drawPolyline(points.data(), points.size());

    // 中间层
    QPen midPen(QColor(accent.red(), accent.green(), accent.blue(), 120), 3);
    midPen.setCapStyle(Qt::RoundCap);
    midPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(midPen);
    painter.drawPolyline(points.data(), points.size());

    // 主线（细线，明亮）
    QPen mainPen(QColor(accent.red(), accent.green(), accent.blue(), 220), 1.5);
    mainPen.setCapStyle(Qt::RoundCap);
    mainPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(mainPen);
    painter.drawPolyline(points.data(), points.size());

    // 中心线（参考线）
    painter.setPen(QColor(255, 255, 255, 30));
    painter.drawLine(0, midY, w, midY);
}
