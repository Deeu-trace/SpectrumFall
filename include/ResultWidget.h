#pragma once

#include <QWidget>

class QPushButton;
class QLabel;

/// 结算页面：得分、各判定数、最大连击、评级(S/A/B/C/D)、重试/返回按钮
class ResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultWidget(QWidget* parent = nullptr);

    /// 设置结算数据
    void setResult(int score, int perfect, int good, int miss, int maxCombo, int totalNotes);

signals:
    void retryRequested();
    void backRequested();

private:
    QLabel* m_scoreLabel;       ///< 总分标签
    QLabel* m_gradeLabel;       ///< 评级标签
    QLabel* m_perfectLabel;     ///< Perfect 数量标签
    QLabel* m_goodLabel;        ///< Good 数量标签
    QLabel* m_missLabel;        ///< Miss 数量标签
    QLabel* m_maxComboLabel;    ///< 最大连击标签
    QPushButton* m_retryBtn;    ///< 重试按钮
    QPushButton* m_backBtn;     ///< 返回按钮

    /// 计算评级
    QString calculateGrade(int score, int totalNotes) const;
};
