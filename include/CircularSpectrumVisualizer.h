#pragma once

#include "VisualizerBase.h"
#include <QVector>

/// 圆形频谱可视化：中心圆环，频谱柱从圆周向外辐射，旋转动画
class CircularSpectrumVisualizer : public VisualizerBase
{
    Q_OBJECT

public:
    explicit CircularSpectrumVisualizer(QWidget* parent = nullptr);

    void setSpectrumData(const QVector<float>& magnitude) override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_segments;           ///< 频谱分段数
    float m_rotationAngle;    ///< 当前旋转角度（度）
};
