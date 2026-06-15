#include "SongSelectWidget.h"
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileInfo>

SongSelectWidget::SongSelectWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(30, 30, 30, 30);

    // 标题
    QLabel* titleLabel = new QLabel(QStringLiteral("选择歌曲"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(32);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // 歌曲列表
    m_songList = new QListWidget(this);
    m_songList->setObjectName("songList");
    layout->addWidget(m_songList, 1);

    // 信息行
    QHBoxLayout* infoLayout = new QHBoxLayout();

    m_infoLabel = new QLabel(QStringLiteral("请选择一首歌曲"), this);
    m_infoLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    infoLayout->addWidget(m_infoLabel, 1);

    m_bpmLabel = new QLabel(QStringLiteral("BPM: --"), this);
    m_bpmLabel->setStyleSheet("color: #bd93f9; background: transparent; font-size: 16px; font-weight: bold;");
    infoLayout->addWidget(m_bpmLabel);

    layout->addLayout(infoLayout);

    // 按钮行
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    m_visualizeBtn = new QPushButton(QStringLiteral("可视化模式"), this);
    m_visualizeBtn->setMinimumSize(150, 40);
    m_visualizeBtn->setEnabled(false);
    m_visualizeBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_visualizeBtn);

    m_gameBtn = new QPushButton(QStringLiteral("开始游戏"), this);
    m_gameBtn->setMinimumSize(150, 40);
    m_gameBtn->setEnabled(false);
    m_gameBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_gameBtn);

    btnLayout->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setMinimumSize(100, 40);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    layout->addLayout(btnLayout);

    // 连接信号
    connect(m_songList, &QListWidget::itemClicked, this, &SongSelectWidget::onSongItemClicked);
    connect(m_visualizeBtn, &QPushButton::clicked, this, &SongSelectWidget::onVisualizeClicked);
    connect(m_gameBtn, &QPushButton::clicked, this, &SongSelectWidget::onGameClicked);
    connect(m_backBtn, &QPushButton::clicked, this, &SongSelectWidget::backRequested);
}

void SongSelectWidget::setSongs(const QStringList& paths)
{
    m_songList->clear();
    for (const QString& path : paths) {
        QFileInfo fi(path);
        QListWidgetItem* item = new QListWidgetItem(fi.fileName(), m_songList);
        item->setData(Qt::UserRole, path);
    }
}

QString SongSelectWidget::selectedSong() const
{
    return m_selectedPath;
}

void SongSelectWidget::setBpmDisplay(float bpm)
{
    if (bpm > 0.0f) {
        m_bpmLabel->setText(QStringLiteral("BPM: %1").arg(static_cast<int>(bpm)));
    } else if (bpm == -2.0f) {
        m_bpmLabel->setText(QStringLiteral("BPM: 分析中..."));
    } else {
        m_bpmLabel->setText(QStringLiteral("BPM: --"));
    }
}

void SongSelectWidget::scanDirectory(const QString& dirPath)
{
    m_songList->clear();
    QDir dir(dirPath);
    if (!dir.exists()) return;

    QStringList filters;
    filters << "*.wav" << "*.mp3" << "*.flac";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo& fi : files) {
        QListWidgetItem* item = new QListWidgetItem(fi.fileName(), m_songList);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
    }
}

void SongSelectWidget::onSongItemClicked()
{
    QListWidgetItem* item = m_songList->currentItem();
    if (!item) return;

    m_selectedPath = item->data(Qt::UserRole).toString();
    m_infoLabel->setText(m_selectedPath);
    m_bpmLabel->setText(QStringLiteral("BPM: 分析中..."));

    m_visualizeBtn->setEnabled(true);
    m_gameBtn->setEnabled(true);

    emit songSelected(m_selectedPath);
}

void SongSelectWidget::onVisualizeClicked()
{
    if (!m_selectedPath.isEmpty()) {
        emit visualizeRequested();
    }
}

void SongSelectWidget::onGameClicked()
{
    if (!m_selectedPath.isEmpty()) {
        emit gameRequested();
    }
}
