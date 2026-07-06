#include "WaterfallVisualizer.h"
#include "ThemeManager.h"
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
    // 颜色映射：暗底 → primary → secondary → 亮白
    value = qBound(0.0f, value, 1.0f);

    ThemePalette p = ThemeManager::instance()->gamePalette();
    QColor c1(p.primary);
    QColor c2(p.secondary);
    QColor bg(p.visBgDeep);

    if (value < 0.25f) {
        // 背景色 → primary (淡入)
        float t = value / 0.25f;
        return QColor(
            static_cast<int>(bg.red()   + (c1.red()   - bg.red())   * t),
            static_cast<int>(bg.green() + (c1.green() - bg.green()) * t),
            static_cast<int>(bg.blue()  + (c1.blue()  - bg.blue())  * t),
            static_cast<int>(40 + 215 * t)
        );
    } else if (value < 0.5f) {
        // primary 全亮
        float t = (value - 0.25f) / 0.25f;
        return QColor(c1.red(), c1.green(), c1.blue(), static_cast<int>(255));
    } else if (value < 0.75f) {
        // primary → secondary
        float t = (value - 0.5f) / 0.25f;
        return QColor(
            static_cast<int>(c1.red()   + (c2.red()   - c1.red())   * t),
            static_cast<int>(c1.green() + (c2.green() - c1.green()) * t),
            static_cast<int>(c1.blue()  + (c2.blue()  - c1.blue())  * t),
            255
        );
    } else {
        // secondary → 亮白
        float t = (value - 0.75f) / 0.25f;
        return QColor(
            static_cast<int>(c2.red()   + (255 - c2.red())   * t),
            static_cast<int>(c2.green() + (255 - c2.green()) * t),
            static_cast<int>(c2.blue()  + (255 - c2.blue())  * t),
            255
        );
    }
}

void WaterfallVisualizer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 背景
    ThemePalette p = ThemeManager::instance()->gamePalette();
    painter.fillRect(rect(), QColor(p.visBgDeep));

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
