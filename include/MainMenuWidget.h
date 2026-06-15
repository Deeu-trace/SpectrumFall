#pragma once

#include <QWidget>

class QPushButton;

/// 主菜单页面：标题 "Rhythm Wave"，选择歌曲/退出按钮，背景动画
class MainMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenuWidget(QWidget* parent = nullptr);

signals:
    void songSelectRequested();
    void exitRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPushButton* m_selectSongBtn;   ///< 选择歌曲按钮
    QPushButton* m_exitBtn;         ///< 退出按钮
    float m_bgPhase;                ///< 背景动画相位
};
