#include "BarSpectrumVisualizer.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>

BarSpectrumVisualizer::BarSpectrumVisualizer(QWidget* parent)
    : VisualizerBase(parent)
    , m_barCount(64)
    , m_peakValues(64, 0.0f)
    , m_peakFallSpeed(64, 0.0f)
{
}

void BarSpectrumVisualizer::setSpectrumData(const QVector<float>& magnitude)
{
    // 更新峰值指示器
    if (m_magnitude.size() > 0) {
        // 峰值下落
        for (int i = 0; i < m_barCount; ++i) {
            int magIdx = i * m_magnitude.size() / m_barCount;
            if (magIdx < m_magnitude.size()) {
                float val = m_magnitude[magIdx];
                if (val > m_peakValues[i]) {
                    m_peakValues[i] = val;
                    m_peakFallSpeed[i] = 0.0f;
                } else {
                    m_peakFallSpeed[i] += 0.001f; // 加速下落
                    m_peakValues[i] -= m_peakFallSpeed[i];
                    if (m_peakValues[i] < 0.0f) m_peakValues[i] = 0.0f;
                }
            }
        }
    }

    m_magnitude = magnitude;
    update();
}

void BarSpectrumVisualizer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    ThemePalette p = ThemeManager::instance()->gamePalette();
    painter.fillRect(rect(), QColor(p.visBg));

    if (m_magnitude.isEmpty()) {
        return;
    }

    int w = width();
    int h = height();
    int barWidth = w / m_barCount - 2;
    if (barWidth < 2) barWidth = 2;
    int spacing = 2;

    for (int i = 0; i < m_barCount; ++i) {
        // 将频谱数据映射到柱子
        int magIdx = i * m_magnitude.size() / m_barCount;
        float val = 0.0f;
        if (magIdx < m_magnitude.size()) {
            val = m_magnitude[magIdx];
        }

        int barHeight = static_cast<int>(val * (h - 20));
        int x = i * (barWidth + spacing) + spacing;
        int y = h - barHeight;

        // 颜色渐变：primary → secondary（主题色）
        QColor c1(p.primary);
        QColor c2(p.secondary);
        float t = qBound(0.0f, val, 1.0f);
        QColor barColor(
            static_cast<int>(c1.red()   + (c2.red()   - c1.red())   * t),
            static_cast<int>(c1.green() + (c2.green() - c1.green()) * t),
            static_cast<int>(c1.blue()  + (c2.blue()  - c1.blue())  * t)
        );

        // 绘制柱子
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawRoundedRect(x, y, barWidth, barHeight, 2, 2);

        // 绘制峰值指示器
        if (i < m_peakValues.size() && m_peakValues[i] > 0.01f) {
            int peakY = h - static_cast<int>(m_peakValues[i] * (h - 20));
            painter.setBrush(QColor(255, 255, 255, 200));
            painter.drawRect(x, peakY - 2, barWidth, 2);
        }
    }
}
