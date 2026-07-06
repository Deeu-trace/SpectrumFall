#define _USE_MATH_DEFINES
#include "CircularSpectrumVisualizer.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

CircularSpectrumVisualizer::CircularSpectrumVisualizer(QWidget* parent)
    : VisualizerBase(parent)
    , m_segments(128)
    , m_rotationAngle(0.0f)
{
}

void CircularSpectrumVisualizer::setSpectrumData(const QVector<float>& magnitude)
{
    m_magnitude = magnitude;
    // 旋转动画
    m_rotationAngle += 0.3f;
    if (m_rotationAngle >= 360.0f) m_rotationAngle -= 360.0f;
    update();
}

void CircularSpectrumVisualizer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    ThemePalette p = ThemeManager::instance()->gamePalette();

    // 背景
    painter.fillRect(rect(), QColor(p.visBg));

    if (m_magnitude.isEmpty()) {
        return;
    }

    int w = width();
    int h = height();
    QPointF center(w / 2.0, h / 2.0);
    float innerRadius = qMin(w, h) * 0.15f;
    float maxOuterRadius = qMin(w, h) * 0.42f;

    // 绘制中心圆环
    QColor glowColor(p.surfaceBorder);
    QRadialGradient centerGlow(center, innerRadius * 1.5);
    centerGlow.setColorAt(0.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 180));
    centerGlow.setColorAt(0.6, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 80));
    centerGlow.setColorAt(1.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(centerGlow);
    painter.drawEllipse(center, innerRadius * 1.5, innerRadius * 1.5);

    // 绘制频谱柱（从圆周向外辐射）
    int segCount = qMin(m_segments, m_magnitude.size());
    float angleStep = 360.0f / segCount;

    for (int i = 0; i < segCount; ++i) {
        int magIdx = i * m_magnitude.size() / segCount;
        float val = m_magnitude[magIdx];

        float barLength = val * (maxOuterRadius - innerRadius);
        float angle = (i * angleStep + m_rotationAngle) * M_PI / 180.0f;

        // 内端点
        QPointF innerPt(
            center.x() + innerRadius * std::cos(angle),
            center.y() + innerRadius * std::sin(angle)
        );

        // 外端点
        QPointF outerPt(
            center.x() + (innerRadius + barLength) * std::cos(angle),
            center.y() + (innerRadius + barLength) * std::sin(angle)
        );

        // 颜色：低频 primary，高频 secondary
        float t = static_cast<float>(i) / segCount;
        QColor c1(p.primary);
        QColor c2(p.secondary);
        QColor barColor(
            static_cast<int>(c1.red()   + (c2.red()   - c1.red())   * t),
            static_cast<int>(c1.green() + (c2.green() - c1.green()) * t),
            static_cast<int>(c1.blue()  + (c2.blue()  - c1.blue())  * t)
        );
        barColor.setAlphaF(0.6f + val * 0.4f);

        QPen pen(barColor, qMax(1.0f, angleStep * innerRadius * M_PI / 180.0f * 0.6f));
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(innerPt, outerPt);
    }

    // 中心文字（可选装饰）
    QColor textColor(p.primary);
    painter.setPen(QColor(textColor.red(), textColor.green(), textColor.blue(), 150));
    QFont font = painter.font();
    font.setPixelSize(static_cast<int>(innerRadius * 0.22));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(center.x() - innerRadius, center.y() - innerRadius,
                            innerRadius * 2, innerRadius * 2),
                     Qt::AlignCenter, QStringLiteral("SpectrumFall"));
}
