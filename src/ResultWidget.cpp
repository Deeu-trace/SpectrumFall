#include "ResultWidget.h"
#include <QPushButton>
#include <QLineEdit>
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

    layout->addSpacing(20);

    // 本次排名提示
    m_rankLabel = new QLabel(QStringLiteral("完成一曲，记入排行榜？"), this);
    m_rankLabel->setAlignment(Qt::AlignCenter);
    m_rankLabel->setStyleSheet("color: #8888aa; font-size: 16px; background: transparent;");
    layout->addWidget(m_rankLabel);

    layout->addSpacing(10);

    // 玩家名 + 记入按钮 行
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignCenter);
    nameLayout->setSpacing(12);

    QLabel* nameLabel = new QLabel(QStringLiteral("玩家名"), this);
    nameLabel->setStyleSheet("color: #bd93f9; font-size: 16px; background: transparent;");
    nameLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(QStringLiteral("Player"));
    m_nameEdit->setMaxLength(20);
    m_nameEdit->setMinimumWidth(220);
    m_nameEdit->setStyleSheet(
        "QLineEdit { color: #ffffff; background-color: #16213e; "
        "border: 2px solid #0f3460; border-radius: 8px; padding: 6px 10px; font-size: 16px; }"
        "QLineEdit:focus { border-color: #00ff88; }");
    nameLayout->addWidget(m_nameEdit);

    m_submitBtn = new QPushButton(QStringLiteral("记入排行榜"), this);
    m_submitBtn->setMinimumSize(150, 40);
    m_submitBtn->setObjectName("actionButton");
    nameLayout->addWidget(m_submitBtn);

    layout->addLayout(nameLayout);

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

    // 记入排行榜：发送玩家名并禁用按钮，防止重复提交
    connect(m_submitBtn, &QPushButton::clicked, this, [this]() {
        QString name = m_nameEdit->text().trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("Player");
            m_nameEdit->setText(name);
        }
        m_submitBtn->setEnabled(false);
        m_submitBtn->setText(QStringLiteral("已记入"));
        emit scoreSubmitted(name);
    });
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

    // 重置记入排行榜状态，供本局重新提交
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText(QStringLiteral("记入排行榜"));
    m_rankLabel->setText(QStringLiteral("完成一曲，记入排行榜？"));
    m_rankLabel->setStyleSheet("color: #8888aa; font-size: 16px; background: transparent;");
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

void ResultWidget::presetName(const QString& name)
{
    m_nameEdit->setText(name.isEmpty() ? QStringLiteral("Player") : name);
}

void ResultWidget::showRank(int rank, int totalInBoard)
{
    if (rank <= 0) {
        m_rankLabel->setText(QStringLiteral("未进入前 50 名，再接再厉！"));
        m_rankLabel->setStyleSheet("color: #e94560; font-size: 16px; background: transparent;");
    } else {
        m_rankLabel->setText(QStringLiteral("本次排名  #%1 / %2").arg(rank).arg(totalInBoard));
        m_rankLabel->setStyleSheet("color: #00ff88; font-size: 18px; font-weight: bold; background: transparent;");
    }
}
