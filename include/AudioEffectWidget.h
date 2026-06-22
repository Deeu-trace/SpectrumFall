#pragma once

#include <QWidget>

class AudioEffectProcessor;
class QSlider;
class QLabel;
class QPushButton;
class QButtonGroup;
class QStackedWidget;
/// 音频特效浮层面板
/// 三种效果（回声/滤波/变调）通过 tab 互斥切换，滑块实时调节
class AudioEffectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioEffectWidget(AudioEffectProcessor* processor, QWidget* parent = nullptr);

    /// 当前是否有特效处于激活状态
    bool hasActiveEffect() const;

signals:
    /// 特效类型或参数发生变化（通知外部 AudioEngine 实时生效）
    void effectChanged();

    /// 请求关闭面板
    void requestClose();

private slots:
    void onTabChanged(int index);
    void onResetClicked();

private:
    void buildUI();
    void selectTab(int index);
    void syncAllParams();

    AudioEffectProcessor* m_processor;

    // Tab 按钮
    QButtonGroup* m_tabGroup;
    QStackedWidget* m_paramStack;

    // Echo 参数
    QSlider* m_echoDelaySlider;
    QSlider* m_echoFeedbackSlider;
    QSlider* m_echoMixSlider;
    QLabel*  m_echoDelayVal;
    QLabel*  m_echoFeedbackVal;
    QLabel*  m_echoMixVal;

    // Filter 参数
    QSlider* m_filterCutoffSlider;
    QSlider* m_filterResonanceSlider;
    QPushButton* m_filterTypeBtn;   // 低通 / 高通 切换
    QLabel*  m_filterCutoffVal;
    QLabel*  m_filterResonanceVal;
    bool m_filterIsLowPass = true;

    // Pitch 参数
    QSlider* m_pitchSlider;
    QLabel*  m_pitchVal;

    int m_currentTab = 0;  // 0=None, 1=Echo, 2=Filter, 3=Pitch
};
