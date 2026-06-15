#pragma once

#include "VisualizerBase.h"
#include <QVector>

/// 波形可视化：实时波形线条，带辉光效果
class WaveformVisualizer : public VisualizerBase
{
    Q_OBJECT

public:
    explicit WaveformVisualizer(QWidget* parent = nullptr);

    void setWaveformData(const QVector<float>& samples) override;

protected:
    void paintEvent(QPaintEvent* event) override;
};
