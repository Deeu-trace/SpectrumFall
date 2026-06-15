#include "ResultWidget.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

ResultWidget::ResultWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(15);
    layout->setContentsMargins(40, 40, 40, 40);

    // 评级
    m_gradeLabel = new QLabel(QStringLiteral("S"), this);
    QFont gradeFont = m_gradeLabel->font();
    gradeFont.setPixelSize(120);
    gradeFont.setBold(true);
    m_gradeLabel->setFont(gradeFont);
    m_gradeLabel->setAlignment(Qt::AlignCenter);
    m_gradeLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(m_gradeLabel);

    // 分数
    m_scoreLabel = new QLabel(QStringLiteral("0"), this);
    QFont scoreFont = m_scoreLabel->font();
    scoreFont.setPixelSize(36);
    scoreFont.setBold(true);
    m_scoreLabel->setFont(scoreFont);
    m_scoreLabel->setAlignment(Qt::AlignCenter);
    m_scoreLabel->setStyleSheet("color: #ffffff; background: transparent;");
    layout->addWidget(m_scoreLabel);

    layout->addSpacing(20);

    // 判定统计面板
    QWidget* statsPanel = new QWidget(this);
    statsPanel->setObjectName("statsPanel");
    QHBoxLayout* statsLayout = new QHBoxLayout(statsPanel);
    statsLayout->setSpacing(30);

    // Perfect
    QVBoxLayout* perfectLayout = new QVBoxLayout();
    QLabel* perfectTitle = new QLabel(QStringLiteral("Perfect"), this);
    perfectTitle->setAlignment(Qt::AlignCenter);
    perfectTitle->setStyleSheet("color: #00ff88; font-size: 14px; background: transparent;");
    m_perfectLabel = new QLabel(QStringLiteral("0"), this);
    m_perfectLabel->setAlignment(Qt::AlignCenter);
    m_perfectLabel->setStyleSheet("color: #00ff88; font-size: 28px; font-weight: bold; background: transparent;");
    perfectLayout->addWidget(perfectTitle);
    perfectLayout->addWidget(m_perfectLabel);
    statsLayout->addLayout(perfectLayout);

    // Good
    QVBoxLayout* goodLayout = new QVBoxLayout();
    QLabel* goodTitle = new QLabel(QStringLiteral("Good"), this);
    goodTitle->setAlignment(Qt::AlignCenter);
    goodTitle->setStyleSheet("color: #ffff66; font-size: 14px; background: transparent;");
    m_goodLabel = new QLabel(QStringLiteral("0"), this);
    m_goodLabel->setAlignment(Qt::AlignCenter);
    m_goodLabel->setStyleSheet("color: #ffff66; font-size: 28px; font-weight: bold; background: transparent;");
    goodLayout->addWidget(goodTitle);
    goodLayout->addWidget(m_goodLabel);
    statsLayout->addLayout(goodLayout);

    // Miss
    QVBoxLayout* missLayout = new QVBoxLayout();
    QLabel* missTitle = new QLabel(QStringLiteral("Miss"), this);
    missTitle->setAlignment(Qt::AlignCenter);
    missTitle->setStyleSheet("color: #e94560; font-size: 14px; background: transparent;");
    m_missLabel = new QLabel(QStringLiteral("0"), this);
    m_missLabel->setAlignment(Qt::AlignCenter);
    m_missLabel->setStyleSheet("color: #e94560; font-size: 28px; font-weight: bold; background: transparent;");
    missLayout->addWidget(missTitle);
    missLayout->addWidget(m_missLabel);
    statsLayout->addLayout(missLayout);

    layout->addWidget(statsPanel);

    layout->addSpacing(10);

    // 最大连击
    m_maxComboLabel = new QLabel(QStringLiteral("最大连击: 0"), this);
    m_maxComboLabel->setAlignment(Qt::AlignCenter);
    m_maxComboLabel->setStyleSheet("color: #bd93f9; font-size: 20px; background: transparent;");
    layout->addWidget(m_maxComboLabel);

    layout->addSpacing(30);

    // 按钮行
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);

    m_retryBtn = new QPushButton(QStringLiteral("重试"), this);
    m_retryBtn->setMinimumSize(150, 45);
    m_retryBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_retryBtn);

    m_backBtn = new QPushButton(QStringLiteral("返回主菜单"), this);
    m_backBtn->setMinimumSize(150, 45);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    layout->addLayout(btnLayout);

    // 连接信号
    connect(m_retryBtn, &QPushButton::clicked, this, &ResultWidget::retryRequested);
    connect(m_backBtn, &QPushButton::clicked, this, &ResultWidget::backRequested);
}

void ResultWidget::setResult(int score, int perfect, int good, int miss, int maxCombo, int totalNotes)
{
    m_scoreLabel->setText(QStringLiteral("得分: %1").arg(score));
    m_perfectLabel->setText(QString::number(perfect));
    m_goodLabel->setText(QString::number(good));
    m_missLabel->setText(QString::number(miss));
    m_maxComboLabel->setText(QStringLiteral("最大连击: %1").arg(maxCombo));

    QString grade = calculateGrade(score, totalNotes);
    m_gradeLabel->setText(grade);

    // 评级颜色
    if (grade == "S") {
        m_gradeLabel->setStyleSheet("color: #00ff88; background: transparent;");
    } else if (grade == "A") {
        m_gradeLabel->setStyleSheet("color: #bd93f9; background: transparent;");
    } else if (grade == "B") {
        m_gradeLabel->setStyleSheet("color: #8be9fd; background: transparent;");
    } else if (grade == "C") {
        m_gradeLabel->setStyleSheet("color: #ffff66; background: transparent;");
    } else {
        m_gradeLabel->setStyleSheet("color: #e94560; background: transparent;");
    }
}

QString ResultWidget::calculateGrade(int score, int totalNotes) const
{
    if (totalNotes <= 0) return QStringLiteral("D");

    int maxScore = totalNotes * 300;
    float ratio = static_cast<float>(score) / static_cast<float>(maxScore);

    if (ratio >= 0.95f) return QStringLiteral("S");
    if (ratio >= 0.80f) return QStringLiteral("A");
    if (ratio >= 0.60f) return QStringLiteral("B");
    if (ratio >= 0.40f) return QStringLiteral("C");
    return QStringLiteral("D");
}
