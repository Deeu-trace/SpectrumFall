#include "WaterfallVisualizer.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

WaterfallVisualizer::WaterfallVisualizer(QWidget* parent)
    : VisualizerBase(parent)
    , m_maxHistory(128)
{
}

void WaterfallVisualizer::setSpectrumData(const QVector<float>& magnitude)
{
    m_magnitude = magnitude;

    // 将当前频谱帧添加到历史
    if (!magnitude.isEmpty()) {
        m_history.append(magnitude);
        if (m_history.size() > m_maxHistory) {
            m_history.removeFirst();
        }
    }

    update();
}

QColor WaterfallVisualizer::energyToColor(float value) const
{
    // 颜色映射：深蓝 → 青 → 绿 → 黄 → 红
    value = qBound(0.0f, value, 1.0f);

    if (value < 0.25f) {
        // 深蓝到青
        float t = value / 0.25f;
        return QColor(0, 0, static_cast<int>(80 + 175 * t), 255);
    } else if (value < 0.5f) {
        // 青到绿
        float t = (value - 0.25f) / 0.25f;
        return QColor(0, static_cast<int>(255 * t), static_cast<int>(255 * (1.0f - t)), 255);
    } else if (value < 0.75f) {
        // 绿到黄
        float t = (value - 0.5f) / 0.25f;
        return QColor(static_cast<int>(255 * t), 255, 0, 255);
    } else {
        // 黄到红
        float t = (value - 0.75f) / 0.25f;
        return QColor(255, static_cast<int>(255 * (1.0f - t)), 0, 255);
    }
}

void WaterfallVisualizer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 背景
    painter.fillRect(rect(), QColor(10, 10, 30));

    if (m_history.isEmpty()) {
        return;
    }

    int w = width();
    int h = height();
    int historyCount = m_history.size();

    // 每行像素高度
    float rowHeight = static_cast<float>(h) / m_maxHistory;

    for (int row = 0; row < historyCount; ++row) {
        const QVector<float>& frame = m_history[row];
        int y = h - static_cast<int>((historyCount - row) * rowHeight);
        int drawH = static_cast<int>(rowHeight) + 1;

        int cols = qMin(frame.size(), w);
        float colWidth = static_cast<float>(w) / frame.size();

        for (int col = 0; col < cols; ++col) {
            int x = static_cast<int>(col * colWidth);
            int drawW = static_cast<int>(colWidth) + 1;
            QColor color = energyToColor(frame[col]);
            painter.fillRect(x, y, drawW, drawH, color);
        }
    }
}
