#include "VisualizerBase.h"

VisualizerBase::VisualizerBase(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 150);
}

void VisualizerBase::setSpectrumData(const QVector<float>& magnitude)
{
    m_magnitude = magnitude;
    update();
}

void VisualizerBase::setWaveformData(const QVector<float>& samples)
{
    m_waveform = samples;
    update();
}
